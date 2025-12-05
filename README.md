# Hospital Appointment & Triage System

**Author:** Lakshita Kalra  
**Course:** Data Structures  
**Assignment:** Capstone Assignment 5  

---

## **Description**
This console application simulates a hospital outpatient department (OPD) workflow, including:

- Patient registration and updates
- Routine appointment booking (circular queue)
- Emergency triage (priority queue / min heap)
- Doctor schedule management (linked list)
- Undo last action (stack)
- Reports for pending/served patients  

It models a real-world scenario using multiple data structures to handle routine and emergency flows efficiently.

---

## **Data Structures Used**

| Component                  | Data Structure                 | Description |
|----------------------------|-------------------------------|------------|
| Routine Appointment Queue  | Circular Queue                | Enqueue/dequeue patient tokens; handles routine appointments |
| Emergency Triage           | Min Heap / Priority Queue     | Lower severity score ⇒ higher priority; preempts routine appointments |
| Doctor Schedule            | Linked List                   | Stores per-doctor slots with start/end time and status |
| Patient Records            | Hash Table (`unordered_map`)  | Stores patient demographics and history; supports CRUD operations |
| Undo Last Action           | Stack                         | Stores last operation; allows reverting changes |

---

## **Time & Space Complexity**

| Operation                       | Time Complexity | Space Complexity |
|---------------------------------|----------------|-----------------|
| Routine Queue enqueue/dequeue    | O(1)           | O(1) per op     |
| Emergency Priority Queue insert  | O(log n)       | O(1) per op; O(n) total |
| Emergency Priority Queue extract | O(log n)       | O(1) per op     |
| Patient hash insert/search       | O(1) average   | O(1) per op; O(m) table |
| Doctor Schedule insert/delete    | O(1) / O(k)    | O(1) per op; O(k) list |
| Undo push/pop                    | O(1)           | O(1) per op; O(u) total |
| Reports (per doctor)             | O(k) per doctor| O(1) per op     |

**Legend:**  
- n = total queued patients  
- k = slots per doctor  
- m = hash table buckets  
- u = number of undo actions  

---

## **Compilation & Run Instructions (Mac / Linux)**

Open terminal and navigate to the project folder:

```bash
cd "/Users/lakshitakalra/Desktop/DSA ASSIGNMENT"
g++ -std=c++20 hospital_system.cpp -o hospital
./hospital
```
Menu:
=== Hospital Appointment & Triage System ===
1. Register/Update Patient
2. Book Slot / Enqueue Routine
3. Emergency In (Triage)
4. Serve Next (doctor)
5. Undo Last Action
6. Reports
7. List Doctor Slots
8. Add Doctor
9. Add Slot to Doctor
0. Exit
Choose option:

How to Use

Register Patient: Enter patient ID, name, and age.

Book Routine Appointment: Enter patient ID, doctor ID, slot ID.

Add Emergency Patient: Enter patient ID and severity score.

Serve Next: Doctor serves next patient (routine or emergency).

Undo: Reverts the last operation.

Reports: Shows pending and served counts for each doctor.

Sample Run 
Choose option: 1
Enter Patient ID: 101
Enter Name: John
Enter Age: 25
Patient registered successfully.

Choose option: 2
Enter Patient ID: 101
Enter Doctor ID: 1
Enter Slot ID: 1
Routine appointment booked.

Choose option: 3
Enter Patient ID: 101
Enter Severity Score (1=High, 10=Low): 2
Emergency patient added.

Choose option: 4
Next patient served: ID=101, Name=John

Choose option: 5
Last action undone.

Choose option: 6
--- Reports ---
Doctor 1: Pending=0, Served=1

References

Mark Allen Weiss, Data Structures and Algorithm Analysis in C++

Goodrich, Tamassia, Goldwasser, Data Structures and Algorithms in Python

GeeksforGeeks: Queues, Heaps, Hashing
