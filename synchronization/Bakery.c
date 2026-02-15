#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <limits.h>

// Bakery capacity and resource limits
#define MAX_CUSTOMERS_IN_BAKERY 25
#define MAX_SOFA 4
#define MAX_CHEFS 4
#define MAX_OVENS 4

// Customer states for clarity
typedef enum {
    ARRIVED, ENTERED, WAITING_FOR_SOFA, SITTING, WAITING_FOR_CAKE,
    CAKE_READY, WAITING_FOR_PAYMENT, PAID, LEFT
} CustomerState;

typedef struct {
    int customer_id;
    long request_time;
} CakeRequest;

typedef struct {
    int id;
    int arrival_time;
    CustomerState state;
    pthread_t thread;
    long pay_request_time;
} Customer;

typedef struct {
    int id;
    pthread_t thread;
} Chef;

typedef struct {
    int customer_id;
    long vacated_time;
} SofaSeat;

// --- SHARED GLOBAL STATE ---
struct {
    pthread_mutex_t lock;
    pthread_cond_t customer_cond;
    pthread_cond_t chef_cond;

    int customers_in_bakery;
    int active_customers;
    int simulation_finished;

    SofaSeat sofa[MAX_SOFA];
    int chefs_baking;
    int payment_counter_busy;

    int sofa_wait_q[MAX_CUSTOMERS_IN_BAKERY];
    CakeRequest cake_req_q[MAX_CUSTOMERS_IN_BAKERY];
    int payment_q[MAX_CUSTOMERS_IN_BAKERY];
    int sofa_wait_count, cake_req_count, payment_count;
    int sofa_wait_front, cake_req_front, payment_front;
    int sofa_wait_rear, cake_req_rear, payment_rear;
} bakery;

struct timeval start_time;
Customer* all_customers_global = NULL;
int customer_count_global = 0;
int min_arrival_time_global = 0; // earliest arrival time from input

// --- Utility Functions ---
long get_timestamp() {
    struct timeval now;
    gettimeofday(&now, NULL);
    // Return simulated time as: elapsed since simulation start + earliest input timestamp.
    // This makes the first events print at the minimum timestamp from input (e.g., 10),
    // and avoids counting the time spent entering/piping input before threads start.
    return (now.tv_sec - start_time.tv_sec) + min_arrival_time_global;
}

void print_action(const char* actor, int id, const char* action) {
    printf("%ld %s %d %s\n", get_timestamp(), actor, id, action);
    fflush(stdout);
}

Customer* find_customer_by_id(int id) {
    for (int i = 0; i < customer_count_global; i++) {
        if (all_customers_global[i].id == id) return &all_customers_global[i];
    }
    return NULL;
}

// --- Thread Functions ---

void* customer_thread(void* arg) {
    Customer* me = (Customer*)arg;
    pthread_mutex_lock(&bakery.lock);
    while (bakery.customers_in_bakery >= MAX_CUSTOMERS_IN_BAKERY) {
        pthread_cond_wait(&bakery.customer_cond, &bakery.lock);
    }
    bakery.customers_in_bakery++;
    bakery.active_customers++;
    me->state = ENTERED;
    print_action("Customer", me->id, "enters");
    pthread_mutex_unlock(&bakery.lock);

    sleep(1);

    pthread_mutex_lock(&bakery.lock);
    me->state = WAITING_FOR_SOFA;
    bakery.sofa_wait_q[bakery.sofa_wait_rear] = me->id;
    bakery.sofa_wait_rear = (bakery.sofa_wait_rear + 1) % MAX_CUSTOMERS_IN_BAKERY;
    bakery.sofa_wait_count++;
    pthread_cond_broadcast(&bakery.chef_cond);
    while (me->state == WAITING_FOR_SOFA) {
        pthread_cond_wait(&bakery.customer_cond, &bakery.lock);
    }
    print_action("Customer", me->id, "sits");
    pthread_mutex_unlock(&bakery.lock);

    sleep(1);

    pthread_mutex_lock(&bakery.lock);
    me->state = WAITING_FOR_CAKE;
    print_action("Customer", me->id, "requests cake");
    CakeRequest req = { .customer_id = me->id, .request_time = get_timestamp() };
    bakery.cake_req_q[bakery.cake_req_rear] = req;
    bakery.cake_req_rear = (bakery.cake_req_rear + 1) % MAX_CUSTOMERS_IN_BAKERY;
    bakery.cake_req_count++;
    pthread_cond_broadcast(&bakery.chef_cond);
    while (me->state == WAITING_FOR_CAKE) {
        pthread_cond_wait(&bakery.customer_cond, &bakery.lock);
    }
    pthread_mutex_unlock(&bakery.lock);

    pthread_mutex_lock(&bakery.lock);
    me->state = WAITING_FOR_PAYMENT;
    me->pay_request_time = get_timestamp();
    print_action("Customer", me->id, "pays");
    bakery.payment_q[bakery.payment_rear] = me->id;
    bakery.payment_rear = (bakery.payment_rear + 1) % MAX_CUSTOMERS_IN_BAKERY;
    bakery.payment_count++;
    pthread_cond_broadcast(&bakery.chef_cond);
    while (me->state == WAITING_FOR_PAYMENT) {
        pthread_cond_wait(&bakery.customer_cond, &bakery.lock);
    }
    pthread_mutex_unlock(&bakery.lock);

    pthread_mutex_lock(&bakery.lock);
    me->state = LEFT;
    print_action("Customer", me->id, "leaves");
    bakery.customers_in_bakery--;
    bakery.active_customers--;
    pthread_cond_broadcast(&bakery.customer_cond);
    pthread_cond_broadcast(&bakery.chef_cond);
    pthread_mutex_unlock(&bakery.lock);

    return NULL;
}


void* chef_thread(void* arg) {
    Chef* me = (Chef*)arg;

    while (1) {
        pthread_mutex_lock(&bakery.lock);

        while (bakery.sofa_wait_count == 0 && bakery.payment_count == 0 && bakery.cake_req_count == 0 && !bakery.simulation_finished) {
            pthread_cond_wait(&bakery.chef_cond, &bakery.lock);
        }

        if (bakery.simulation_finished && bakery.sofa_wait_count == 0 && bakery.payment_count == 0 && bakery.cake_req_count == 0) {
            pthread_mutex_unlock(&bakery.lock);
            break;
        }

        // Seating customers
        if (bakery.sofa_wait_count > 0) {
            for (int i = 0; i < MAX_SOFA; ++i) {    
                if (bakery.sofa[i].customer_id == 0 && get_timestamp() >= bakery.sofa[i].vacated_time) {
                    if (bakery.sofa_wait_count > 0) {
                        int cust_id = bakery.sofa_wait_q[bakery.sofa_wait_front];
                        bakery.sofa_wait_front = (bakery.sofa_wait_front + 1) % MAX_CUSTOMERS_IN_BAKERY;
                        bakery.sofa_wait_count--;
                        Customer* cust = find_customer_by_id(cust_id);
                        if (cust) {
                            bakery.sofa[i].customer_id = cust_id;
                            cust->state = SITTING;
                            pthread_cond_broadcast(&bakery.customer_cond);
                        }
                    } else break;
                }
            }
        }

        // PRIORITY 1: Payment
        if (bakery.payment_count > 0 && !bakery.payment_counter_busy) {
            int next_cust_id = bakery.payment_q[bakery.payment_front];
            Customer* cust_to_pay = find_customer_by_id(next_cust_id);
            if (cust_to_pay && get_timestamp() >= cust_to_pay->pay_request_time + 1) {
                bakery.payment_counter_busy = 1;
                bakery.payment_front = (bakery.payment_front + 1) % MAX_CUSTOMERS_IN_BAKERY;
                bakery.payment_count--;

                pthread_mutex_unlock(&bakery.lock);
                printf("%ld Chef %d accepts payment for Customer %d\n", get_timestamp(), me->id, next_cust_id);
                fflush(stdout);
                sleep(2);
                pthread_mutex_lock(&bakery.lock);

                cust_to_pay->state = PAID;
                for (int i = 0; i < MAX_SOFA; i++) {
                    if (bakery.sofa[i].customer_id == next_cust_id) {
                        bakery.sofa[i].customer_id = 0;
                        bakery.sofa[i].vacated_time = get_timestamp();
                        break;
                    }
                }
                pthread_cond_broadcast(&bakery.customer_cond);
                bakery.payment_counter_busy = 0;
                pthread_cond_broadcast(&bakery.chef_cond);
            }
        }
        
        // CRITICAL FIX: Changed 'else if' to 'if'
        // PRIORITY 2: Baking
        if (bakery.cake_req_count > 0 && bakery.chefs_baking < MAX_OVENS) {
            CakeRequest* next_req = &bakery.cake_req_q[bakery.cake_req_front];
            if (get_timestamp() >= next_req->request_time + 1) {
                bakery.chefs_baking++;
                int cust_id = next_req->customer_id;
                bakery.cake_req_front = (bakery.cake_req_front + 1) % MAX_CUSTOMERS_IN_BAKERY;
                bakery.cake_req_count--;

                pthread_mutex_unlock(&bakery.lock);
                printf("%ld Chef %d bakes for Customer %d\n", get_timestamp(), me->id, cust_id);
                fflush(stdout);
                sleep(2);
                pthread_mutex_lock(&bakery.lock);
                
                Customer* cust = find_customer_by_id(cust_id);
                if (cust) {
                    cust->state = CAKE_READY;
                    pthread_cond_broadcast(&bakery.customer_cond);
                }
                bakery.chefs_baking--;
                pthread_cond_broadcast(&bakery.chef_cond);
            }
        }

        pthread_mutex_unlock(&bakery.lock);
        usleep(10000);
    }
    return NULL;
}


int main() {
    pthread_mutex_init(&bakery.lock, NULL);
    pthread_cond_init(&bakery.customer_cond, NULL);
    pthread_cond_init(&bakery.chef_cond, NULL);

    bakery.customers_in_bakery = 0;
    bakery.active_customers = 0;
    bakery.simulation_finished = 0;
    bakery.chefs_baking = 0;
    bakery.payment_counter_busy = 0;
    bakery.sofa_wait_count = bakery.cake_req_count = bakery.payment_count = 0;
    bakery.sofa_wait_front = bakery.cake_req_front = bakery.payment_front = 0;
    bakery.sofa_wait_rear = bakery.cake_req_rear = bakery.payment_rear = 0;
    for (int i = 0; i < MAX_SOFA; i++) {
        bakery.sofa[i].customer_id = 0;
        bakery.sofa[i].vacated_time = -1;
    }

    static Customer all_customers[200];
    all_customers_global = all_customers;
    char line[100];
    int min_arrival_time = INT_MAX;
    while (fgets(line, sizeof(line), stdin)) {
        if (strstr(line, "<EOF>") != NULL) break;
        int timestamp, id;
        char type[20];
        if (sscanf(line, "%d %s %d", &timestamp, type, &id) == 3) {
            all_customers[customer_count_global].id = id;
            all_customers[customer_count_global].arrival_time = timestamp;
            all_customers[customer_count_global].state = ARRIVED;
            all_customers[customer_count_global].pay_request_time = -1;
            customer_count_global++;
            if (timestamp < min_arrival_time) min_arrival_time = timestamp;
        }
    }

    // Initialize simulation clock AFTER reading input, so interactive input time isn't counted.
    // Also set the base (offset) so logs start from earliest input timestamp (e.g., 10).
    if (min_arrival_time == INT_MAX) {
        // No valid input; default to 0
        min_arrival_time = 0;
    }
    min_arrival_time_global = min_arrival_time;
    gettimeofday(&start_time, NULL);

    Chef chefs[MAX_CHEFS];
    for (int i = 0; i < MAX_CHEFS; i++) {
        chefs[i].id = i + 1;
        pthread_create(&chefs[i].thread, NULL, chef_thread, &chefs[i]);
    }

    for (int i = 0; i < customer_count_global; i++) {
        while (get_timestamp() < all_customers[i].arrival_time) {
            usleep(50000);
        }
        pthread_create(&all_customers[i].thread, NULL, customer_thread, &all_customers[i]);
    }

    pthread_mutex_lock(&bakery.lock);
    while (bakery.active_customers > 0) {
        pthread_cond_wait(&bakery.chef_cond, &bakery.lock);
    }
    bakery.simulation_finished = 1;
    pthread_cond_broadcast(&bakery.chef_cond);
    pthread_mutex_unlock(&bakery.lock);

    for (int i = 0; i < customer_count_global; i++) {
        pthread_join(all_customers[i].thread, NULL);
    }
    for (int i = 0; i < MAX_CHEFS; i++) {
        pthread_join(chefs[i].thread, NULL);
    }

    pthread_mutex_destroy(&bakery.lock);
    pthread_cond_destroy(&bakery.customer_cond);
    pthread_cond_destroy(&bakery.chef_cond);

    return 0;
}