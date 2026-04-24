# CSEN 602 — Operating Systems: Project Description
**German University in Cairo | Faculty of Media Engineering and Technology**
**Instructor:** Dr. Aya Salama | **Semester:** Spring 2026
**Due Date:** April 18, 2026 at 11:59 PM
**Team Size:** 4–5 members | **Language:** Java (mandatory GUI) or C (optional GUI bonus)

---

## Project Objective

Build a simulation of an operating system to understand how an OS manages resources and processes. Graded on both **correct implementation** and **correct architecture**.

---

## High-Level Description

- Implement a **basic interpreter** that reads `.txt` program files and executes them as processes.
- Implement a **memory** system to store processes.
- Implement **mutexes** for mutual exclusion over critical resources.
- Implement a **scheduler** to schedule processes in the system.

---

## System Calls

System calls are the process's way of requesting a service from the OS. Required system calls:

| # | System Call |
|---|-------------|
| 1 | Read data from a file on disk |
| 2 | Write text output to a file on disk |
| 3 | Print data on screen |
| 4 | Take text input from the user |
| 5 | Read data from memory |
| 6 | Write data to memory |

---

## Memory

- **Total size:** 40 memory words (fixed).
- Each **memory word** stores 1 variable and its corresponding data.
- Memory is large enough to hold unparsed lines of code, variables, and PCB for any process.
- Each process needs space for **3 variables**.
- Use any naming convention for variable names associated with lines of code and PCB elements.
- Processes **must not** access data outside their allocated memory block.
- Lines of code, variables, and PCB may be separated within memory as long as they fall within the same data structure representing memory.

### Process Creation Rules
- A process is only created at its **arrival time**.
- A process is considered created when:
  1. Its program file is read into lines.
  2. It is assigned a portion of memory for instructions, variables, and PCB.

### Memory Overflow & Swapping
- When a new process is created, the system checks if there is enough space.
- If **not enough space**: unload one of the existing processes and store its data on disk.
- The format for memory stored on disk is free to be defined.
- While unloaded, the process **remains in the scheduler**.
- When it is time for an unloaded process to run again, its memory is **swapped back** from disk into memory.
- You must implement:
  - Writing an existing process's data to disk.
  - Reading an existing process's data from disk.
  - Managing swapping between processes.
  - Protecting each process's memory.

---

## Process Control Block (PCB)

Each process must have a PCB stored in memory containing:

| Field | Description |
|-------|-------------|
| Process ID | Assigned when the process is created |
| Process State | (e.g., Ready, Running, Blocked, Finished) |
| Program Counter | Current instruction pointer |
| Memory Boundaries | Start and end of the process's allocated memory |

---

## Programs

There are **3 provided program files**, each representing a process:

| Program | Description |
|---------|-------------|
| Program 1 | Given 2 numbers, prints all numbers between them on screen |
| Program 2 | Given a filename and data, writes the data to that file (file does not exist; always create it) |
| Program 3 | Given a filename, prints the contents of the file on screen |

---

## Program Syntax (Interpreter Instructions)

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| `print` | `print x` | Prints the value of variable `x` to the screen |
| `assign` | `assign x y` | Initializes variable `x` and assigns value `y`. If `y` is `input`, prints `"Please enter a value"` and reads from user input |
| `writeFile` | `writeFile x y` | Writes data `y` to file `x` |
| `readFile` | `readFile x` | Reads and returns data from file `x` |
| `printFromTo` | `printFromTo x y` | Prints all integers from `x` to `y` |
| `semWait` | `semWait x` | Acquires the mutex/resource `x` |
| `semSignal` | `semSignal x` | Releases the mutex/resource `x` |

---

## Mutual Exclusion (Mutexes)

A mutex controls access to a shared resource between processes. Implement using `semWait` and `semSignal` as atomic operations.

### Required Mutexes (one per resource):

| Resource Name | Protects |
|---------------|----------|
| `userInput` | Taking input from the user |
| `userOutput` | Printing output to the screen |
| `file` | Reading from or writing to a file |

### Rules:
- **Only ONE process** may use a resource at a time.
- If a process requests a resource that is already in use:
  - It is **blocked**.
  - It is added to the **blocked queue of that resource**.
  - It is added to the **general blocked queue**.

### Usage Example (printing to screen):
```
semWait userOutput
print x
semSignal userOutput
```

---

## Scheduler

The scheduler selects processes from the Ready Queue to execute. You must implement **multiple scheduling algorithms**.

### Process Arrival Times (default):
| Process | Arrival Time |
|---------|--------------|
| Process 1 | Time 0 |
| Process 2 | Time 1 |
| Process 3 | Time 4 |

> ⚠️ Arrival times are subject to change during evaluation.

---

### Scheduling Algorithms

#### 1. Highest Response Ratio Next (HRRN) — *Project Grade*
- **Type:** Non-preemptive
- **Selection:** Process with the highest Response Ratio in the Ready Queue
- **Formula:**
  ```
  Response Ratio = (Waiting Time + Burst Time) / Burst Time
  ```
- At every scheduling decision, pick the process with the highest response ratio.

---

#### 2. Round Robin (RR) — *Project Grade*
- **Type:** Preemptive
- **Time Slice:** Each process executes **2 instructions per time slice** (subject to change during evaluation).
- If a process does not finish within its time slice, it is moved to the **end of the Ready Queue**.

---

#### 3. Multi-Level Feedback Queue (MLFQ) — *Assignment Grade (5%), NOT Project Grade*
- **Number of Queues:** 4 priority queues
- Processes start in the **highest-priority queue** (index 0).
- **Quantum formula:** `2^i` where `i` is the queue level index (starting from 0).
  - Queue 0: quantum = 1
  - Queue 1: quantum = 2
  - Queue 2: quantum = 4
  - Queue 3: quantum = 8 (scheduled using Round Robin)
- If a process uses its entire quantum → moved to the **next lower-priority queue**.
- Scheduler always picks from the **highest-priority non-empty queue**.
- The **last queue** (Queue 3) is scheduled using **Round Robin**.

---

## Queues

| Queue | Purpose |
|-------|---------|
| Ready Queue | Processes waiting to be selected for execution |
| Blocked Queue | Processes waiting for a resource to become available |

---

## Required Output

The simulated OS must produce the following outputs (must be **readable and presentable**):

| Output | Trigger |
|--------|---------|
| State of all queues | After every scheduling event (process chosen, blocked, or finished) |
| Currently executing process | Every cycle |
| Currently executing instruction | Every cycle |
| Memory contents (human-readable) | Every clock cycle |
| ID of any process swapped in/out of disk | On every swap |
| Format of memory stored on disk | Visible during simulation |

> ⚠️ The following are **subject to change** during evaluation:
> - Time slice size (may be asked to change to `x` instructions per time slice)
> - Order in which processes are scheduled
> - Timings at which processes arrive

---

## Graphical User Interface (GUI)

| Language | GUI Requirement |
|----------|----------------|
| Java | **Mandatory** — part of project evaluation |
| C | **Optional** — bonus component |

### Recommended GUI Features:
- Visual display of the **Ready Queue** and **Blocked Queue**
- Display of the **currently running process**
- Visualization of **memory contents** and **disk swapping**
- **Step-by-step clock cycle** execution view
- Clear indication of **process state changes** (Ready → Running → Blocked → Finished)
- Controls to **start, pause, or step through** execution

---

## Work Distribution (Suggested Modules)

| Module |
|--------|
| Code Parser / Interpreter |
| System Calls |
| Mutexes |
| Scheduler |
| Memory |

---

## Submission

- **Format:** One `.zip` folder
- **Contents:** All source files (`.java` or `.c`)
- **Naming:** `Team_XX` (e.g., `Team_00`)
- **Late submissions:** Not accepted
