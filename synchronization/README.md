# Office Bakery Synchronization Problem

## Overview

This project implements a multi-threaded simulation of an office bakery where customers and chefs interact using proper synchronization primitives. It demonstrates advanced concepts in concurrent programming including mutexes, condition variables, resource management, and deadlock-free coordination.

## Problem Description

### Scenario
An office has a bakery with the following resources and constraints:
- **4 Ovens** - Maximum 4 cakes can be baked simultaneously
- **4 Chefs** - 4 concurrent workers serving customers
- **4 Sofa Seats** - Limited waiting area for customers
- **25 Customer Capacity** - Maximum total customers in bakery at once
- **1 Payment Counter** - Single serialized payment processing

### Customer Journey
Each customer follows this sequence:
1. **Enter** the bakery (if under capacity)
2. **Wait for sofa** seat
3. **Sit** on sofa (takes 1 second)
4. **Request** a cake
5. **Wait** for cake to be baked (chef takes 2 seconds)
6. **Pay** at counter (takes 2 seconds, includes chef processing time)
7. **Leave** bakery (frees sofa and capacity)

### Chef Responsibilities
Chefs handle three types of tasks (in priority order):
1. **Accept Payment** - Highest priority, one chef at a time
2. **Bake Cakes** - Up to 4 chefs can bake concurrently
3. **Seat Customers** - Assign customers to available sofas

## Technical Implementation

### Data Structures

#### Customer Structure
```c
struct Customer {
    int id;                    // Customer ID
    int arrival_time;          // When customer arrives
    enum CustomerState state;  // Current state in bakery
    pthread_t thread;          // Thread handle
    int pay_request_time;      // When payment initiated
};

enum CustomerState {
    ARRIVED, ENTERED, WAITING_FOR_SOFA, SITTING,
    WAITING_FOR_CAKE, CAKE_READY, WAITING_FOR_PAYMENT,
    PAID, LEFT
};
```

#### Chef Structure
```c
struct Chef {
    int id;           // Chef ID
    pthread_t thread; // Thread handle
};
```

#### Shared Bakery State
```c
struct Bakery {
    pthread_mutex_t lock;              // Protects all shared state
    pthread_cond_t customer_cond;      // Wakes customers on state change
    pthread_cond_t chef_cond;          // Wakes chefs when work available
    
    int customers_in_bakery;           // Current customer count
    int active_customers;              // Customers not yet left
    bool simulation_finished;          // All customers processed
    
    SofaSeat sofa[4];                 // Sofa seat array
    
    // Circular queues
    CircularQueue sofa_wait_q;        // Customers waiting for sofa
    CircularQueue cake_req_q;         // Cake requests
    CircularQueue payment_q;          // Customers ready to pay
    
    int chefs_baking;                 // Current baking count
    bool payment_counter_busy;        // Cash register lock
};
```

### Synchronization Mechanisms

#### 1. Mutex for Mutual Exclusion
All shared state protected by single global mutex:
- Simplifies reasoning about critical sections
- Prevents race conditions on counters and queues
- Must be released during blocking operations (sleep)

#### 2. Condition Variables
**Customer Condition (`customer_cond`):**
- Signaled when customer's state changes
- Woken when sofa available, cake ready, payment processed

**Chef Condition (`chef_cond`):**
- Signaled when new work arrives
- Woken when customer added to any queue

#### 3. State Machine Pattern
Customers progress through well-defined states:
```
ARRIVED → ENTERED → WAITING_FOR_SOFA → SITTING →
WAITING_FOR_CAKE → CAKE_READY → WAITING_FOR_PAYMENT →
PAID → LEFT
```

## Building and Running

### Compilation
```bash
make
```

### Execution
```bash
# Interactive input
./Bakery
10 Customer 1
12 Customer 2
15 Customer 3
^D

# Or with input file
./Bakery < input.txt
```

### Input Format
```
<timestamp> Customer <id>
```

Example:
```
10 Customer 1
10 Customer 2
15 Customer 3
20 Customer 4
```

### Output Format
```
10 Customer 1 enters the bakery
11 Customer 1 sits on sofa
12 Customer 1 requests for cake
14 Customer 1 pays the bill
16 Customer 1 leaves the bakery
```

## Key Synchronization Patterns

### 1. Producer-Consumer
- Customers produce requests (sofa, cake, payment)
- Chefs consume requests from queues

### 2. Resource Pool Management
- Limited resources (sofas, ovens, payment counter)
- Fair allocation using FIFO queues

### 3. State-Based Coordination
- State machines prevent invalid transitions
- Condition variables synchronize state changes

### 4. Timed Coordination
- Enforcing minimum delays between related actions
- Timestamp-based request validation

## Avoiding Common Pitfalls

### Deadlock Prevention
- Single mutex prevents circular wait
- Always release mutex before sleep
- Broadcast instead of signal to wake all waiters

### Race Conditions
- All shared state access protected by mutex
- Atomic state transitions

### Lost Wakeups
- Use `pthread_cond_broadcast` (not signal)
- Always check condition in while loop:
```c
while (condition_not_met) {
    pthread_cond_wait(&cond, &mutex);
}
```

### Priority Inversion
- Explicit task prioritization in chef logic
- Payment > Baking > Seating

## Learning Outcomes

1. **Mutex Usage**: Protecting critical sections
2. **Condition Variables**: Waiting for events efficiently
3. **Resource Management**: Fair allocation with limited resources
4. **State Machines**: Coordinating complex multi-step processes
5. **Timing**: Enforcing delays and scheduling in concurrent systems
6. **Deadlock Avoidance**: Preventing circular dependencies
7. **Queue Management**: Producer-consumer with multiple queues
8. **Thread Lifecycle**: Creation, coordination, and termination

## Files

- `Bakery.c` - Complete implementation with customer and chef threads
- `README.md` - This documentation

## References

- POSIX Threads (pthreads) documentation
- Producer-Consumer problem
- Readers-Writers problem
- Dining Philosophers problem (resource allocation)
- Synchronization patterns in concurrent systems

---

This implementation demonstrates production-quality synchronization code with proper resource management, graceful error handling, and deterministic behavior suitable for testing and analysis.
