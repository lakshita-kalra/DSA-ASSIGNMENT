// hospital_system.cpp
// Build with: clang++ -std=gnu++14 hospital_system.cpp -o hospital_system
// Or: g++ -std=gnu++14 hospital_system.cpp -o hospital_system
// Run: ./hospital_system

#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <stack>
#include <unordered_map>
#include <algorithm>

using namespace std;

/*
 Hospital Appointment & Triage System
 - Doctor schedules: singly linked list (SlotNode)
 - Routine appointments: per-doctor circular queue (vector buffer)
 - Emergency triage: min-heap (priority_queue with greater comparator)
 - Patient index: unordered_map<int, Patient>
 - Undo log: stack<Action>
 Note: this file targets C++14 (no std::optional).
*/

// ----------------------------- ADTs -----------------------------
enum TokenType { ROUTINE, EMERGENCY };

struct Patient {
    int id = 0;
    string name;
    int age = 0;
    string history;
    int freq = 0;
};

struct Token {
    int tokenId = -1;
    int patientId = -1;
    int doctorId = -1;
    int slotId = -1; // -1 if not slot-based
    TokenType type = ROUTINE;
};

// Singly linked list node for slots
struct SlotNode {
    int slotId;
    string startTime;
    string endTime;
    bool taken;
    int tokenId;
    SlotNode* next;
    SlotNode(int sid, const string& s, const string& e)
        : slotId(sid), startTime(s), endTime(e), taken(false), tokenId(-1), next(nullptr) {}
};

// ----------------------------- Doctor -----------------------------
struct Doctor {
    int id = 0;
    string name;
    string specialization;
    SlotNode* slotHead = nullptr;
    vector<Token> circBuffer;
    int frontIdx = 0, rearIdx = -1;
    int capacity = 10;
    int sizeQ = 0;

    Doctor() = default;
    Doctor(int _id, const string& _name, const string& _spec, int cap = 10)
        : id(_id), name(_name), specialization(_spec), slotHead(nullptr), circBuffer(cap),
          capacity(cap), frontIdx(0), rearIdx(-1), sizeQ(0) {}

    ~Doctor() {
        SlotNode* cur = slotHead;
        while (cur) {
            SlotNode* nxt = cur->next;
            delete cur;
            cur = nxt;
        }
        slotHead = nullptr;
    }

    bool isFull() const { return sizeQ == capacity; }
    bool isEmpty() const { return sizeQ == 0; }

    bool enqueueRoutine(const Token& t) {
        if (isFull()) return false;
        rearIdx = (rearIdx + 1) % capacity;
        circBuffer[rearIdx] = t;
        sizeQ++;
        return true;
    }

    bool dequeueRoutine(Token& out) {
        if (isEmpty()) return false;
        out = circBuffer[frontIdx];
        frontIdx = (frontIdx + 1) % capacity;
        sizeQ--;
        if (sizeQ == 0) { frontIdx = 0; rearIdx = -1; }
        return true;
    }

    bool peekRoutine(Token& out) const {
        if (isEmpty()) return false;
        out = circBuffer[frontIdx];
        return true;
    }

    int pendingCount() const { return sizeQ; }

    void insertSlot(int slotId, const string& s, const string& e) {
        SlotNode* node = new SlotNode(slotId, s, e);
        if (!slotHead) { slotHead = node; return; }
        SlotNode* cur = slotHead;
        while (cur->next) cur = cur->next;
        cur->next = node;
    }

    bool cancelSlot(int slotId) {
        SlotNode* cur = slotHead;
        SlotNode* prev = nullptr;
        while (cur) {
            if (cur->slotId == slotId) {
                if (prev) prev->next = cur->next;
                else slotHead = cur->next;
                delete cur;
                return true;
            }
            prev = cur; cur = cur->next;
        }
        return false;
    }

    SlotNode* findSlot(int slotId) {
        SlotNode* cur = slotHead;
        while (cur) {
            if (cur->slotId == slotId) return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    SlotNode* nextFreeSlot() {
        SlotNode* cur = slotHead;
        while (cur) {
            if (!cur->taken) return cur;
            cur = cur->next;
        }
        return nullptr;
    }

    void printSlots() {
        cout << "Slots for Dr. " << name << " (id " << id << "):\n";
        SlotNode* cur = slotHead;
        while (cur) {
            cout << "  SlotId: " << cur->slotId << " [" << cur->startTime << "-" << cur->endTime << "]"
                 << (cur->taken ? " (TAKEN)" : " (FREE)") << "\n";
            cur = cur->next;
        }
    }
};

// ----------------------------- Emergency Triage -----------------------------
struct TriagedToken {
    int severity;
    Token token;
    bool operator>(TriagedToken const& other) const {
        if (severity != other.severity) return severity > other.severity;
        return token.tokenId > other.token.tokenId;
    }
};

// ----------------------------- Undo Stack -----------------------------
enum ActionType { BOOK, CANCEL, SERVE, REGISTER_PATIENT, TRIAGE_INSERT };

struct Action {
    ActionType type;
    Token token;
    int slotId = -1;
    int doctorId = -1;
    Patient patientSnapshot;
    bool slotPreviouslyTaken = false;
    int severity = 0;
    int patientIdForUpsert = -1;
    bool patientExistedBefore = false;
};

// ----------------------------- HospitalSystem -----------------------------
class HospitalSystem {
private:
    unordered_map<int, Doctor> doctors;
    unordered_map<int, Patient> patients;
    priority_queue<TriagedToken, vector<TriagedToken>, greater<TriagedToken>> triageHeap;
    stack<Action> undoStack;
    int nextTokenId = 1;
    int servedCount = 0;
    int pendingCountTotal = 0;

public:
    HospitalSystem() = default;

    bool addDoctor(int docId, const string& name, const string& spec, int queueCap = 10) {
        if (doctors.count(docId)) return false;
        doctors.emplace(docId, Doctor(docId, name, spec, queueCap));
        return true;
    }

    bool scheduleAddSlot(int doctorId, int slotId, const string& startTime, const string& endTime) {
        auto it = doctors.find(doctorId);
        if (it == doctors.end()) return false;
        it->second.insertSlot(slotId, startTime, endTime);
        return true;
    }

    bool scheduleCancelSlot(int doctorId, int slotId) {
        auto it = doctors.find(doctorId); if (it == doctors.end()) return false;
        SlotNode* slot = it->second.findSlot(slotId); if (!slot) return false;
        if (slot->taken) {
            Action act; act.type = CANCEL;
            act.token = Token{slot->tokenId, -1, doctorId, slotId, ROUTINE};
            act.slotId = slotId; act.doctorId = doctorId; act.slotPreviouslyTaken = true;
            undoStack.push(act);
            --pendingCountTotal;
            slot->taken = false; slot->tokenId = -1;
        }
        return it->second.cancelSlot(slotId);
    }

    void patientUpsert(const Patient& p) {
        bool existed = patients.find(p.id) != patients.end();
        Action act; act.type = REGISTER_PATIENT;
        act.patientExistedBefore = existed;
        act.patientIdForUpsert = p.id;
        act.patientSnapshot = existed ? patients[p.id] : Patient();
        undoStack.push(act);
        patients[p.id] = p;
    }

    bool patientGet(int patientId, Patient& out) {
        auto it = patients.find(patientId);
        if (it == patients.end()) return false;
        out = it->second;
        return true;
    }

    int enqueueRoutine(int patientId, int doctorId, int slotId = -1) {
        auto dit = doctors.find(doctorId); if (dit == doctors.end()) return -1;
        if (patients.find(patientId) == patients.end()) return -1;
        Doctor& D = dit->second;
        Token tk; tk.tokenId = nextTokenId++; tk.patientId = patientId; tk.doctorId = doctorId; tk.slotId = slotId; tk.type = ROUTINE;
        if (slotId != -1) {
            SlotNode* slot = D.findSlot(slotId); if (!slot || slot->taken) return -1;
            slot->taken = true; slot->tokenId = tk.tokenId;
            Action act; act.type = BOOK; act.token = tk; act.slotId = slotId; act.doctorId = doctorId; undoStack.push(act);
            ++pendingCountTotal; ++patients[patientId].freq; return tk.tokenId;
        } else {
            if (D.isFull()) return -1;
            D.enqueueRoutine(tk);
            Action act; act.type = BOOK; act.token = tk; act.doctorId = doctorId; undoStack.push(act);
            ++pendingCountTotal; ++patients[patientId].freq; return tk.tokenId;
        }
    }

    bool serveNext(int doctorId, Token& servedOut) {
        if (!triageHeap.empty()) {
            TriagedToken tt = triageHeap.top(); triageHeap.pop();
            Token served = tt.token; served.type = EMERGENCY;
            ++servedCount; --pendingCountTotal;
            Action act; act.type = SERVE; act.token = served; act.severity = tt.severity; undoStack.push(act);
            if (served.patientId != -1) ++patients[served.patientId].freq;
            servedOut = served;
            return true;
        }
        auto dit = doctors.find(doctorId); if (dit == doctors.end()) return false;
        Doctor& D = dit->second;
        Token maybeTk;
        if (!D.dequeueRoutine(maybeTk)) {
            SlotNode* s = D.slotHead;
            while (s) {
                if (s->taken) {
                    Token served; served.tokenId = s->tokenId; served.patientId = -1; served.doctorId = doctorId; served.slotId = s->slotId; served.type = ROUTINE;
                    s->taken = false; s->tokenId = -1;
                    ++servedCount; --pendingCountTotal;
                    Action act; act.type = SERVE; act.token = served; undoStack.push(act);
                    servedOut = served;
                    return true;
                }
                s = s->next;
            }
            return false;
        } else {
            Token served = maybeTk;
            ++servedCount; --pendingCountTotal;
            Action act; act.type = SERVE; act.token = served; undoStack.push(act);
            servedOut = served;
            return true;
        }
    }

    bool triageInsert(int patientId, int severity) {
        if (!patients.count(patientId)) return false;
        Token tk; tk.tokenId = nextTokenId++; tk.patientId = patientId; tk.doctorId = -1; tk.slotId = -1; tk.type = EMERGENCY;
        triageHeap.push(TriagedToken{severity, tk});
        Action act; act.type = TRIAGE_INSERT; act.token = tk; act.severity = severity; undoStack.push(act);
        ++pendingCountTotal; ++patients[patientId].freq; return true;
    }

    bool undoPop() {
        if (undoStack.empty()) return false;
        Action act = undoStack.top(); undoStack.pop();
        switch (act.type) {
            case BOOK: {
                Token tk = act.token;
                auto dit = doctors.find(act.doctorId); if (dit == doctors.end()) return false;
                Doctor& D = dit->second;
                if (tk.slotId != -1) {
                    SlotNode* slot = D.findSlot(tk.slotId);
                    if (slot && slot->taken && slot->tokenId == tk.tokenId) {
                        slot->taken = false;
                        slot->tokenId = -1;
                        --pendingCountTotal;
                        return true;
                    }
                } else {
                    vector<Token> tmp;
                    bool removed = false;
                    Token ot;
                    while (D.dequeueRoutine(ot)) {
                        if (!removed && ot.tokenId == tk.tokenId) { removed = true; --pendingCountTotal; }
                        else tmp.push_back(ot);
                    }
                    for (auto &t: tmp) D.enqueueRoutine(t);
                    return removed;
                }
                return false;
            }
            case CANCEL: {
                auto dit = doctors.find(act.doctorId); if (dit == doctors.end()) return false;
                Doctor& D = dit->second; SlotNode* slot = D.findSlot(act.slotId);
                if (slot) { slot->taken = true; slot->tokenId = act.token.tokenId; ++pendingCountTotal; return true; }
                return false;
            }
            case SERVE: {
                Token tk = act.token;
                if (tk.type == EMERGENCY) {
                    triageHeap.push(TriagedToken{act.severity, tk});
                    ++pendingCountTotal; --servedCount;
                    return true;
                } else {
                    auto dit = doctors.find(tk.doctorId); if (dit == doctors.end()) return false;
                    Doctor& D = dit->second; D.enqueueRoutine(tk);
                    ++pendingCountTotal; --servedCount;
                    return true;
                }
            }
            case REGISTER_PATIENT: {
                if (act.patientExistedBefore) {
                    patients[act.patientIdForUpsert] = act.patientSnapshot;
                } else {
                    patients.erase(act.patientIdForUpsert);
                }
                return true;
            }
            case TRIAGE_INSERT: {
                int remId = act.token.tokenId; vector<TriagedToken> all;
                bool removed = false;
                while (!triageHeap.empty()) {
                    TriagedToken t = triageHeap.top(); triageHeap.pop();
                    if (!removed && t.token.tokenId == remId) { removed = true; --pendingCountTotal; }
                    else all.push_back(t);
                }
                for (auto &x: all) triageHeap.push(x);
                return removed;
            }
            default: return false;
        }
    }

    void perDoctorReport(int doctorId) {
        auto dit = doctors.find(doctorId);
        if (dit == doctors.end()) { cout << "Doctor not found\n"; return; }
        Doctor& D = dit->second;
        cout << "Doctor: " << D.name << " (id " << D.id << "), Spec: " << D.specialization << "\n";
        cout << "Pending routine queue: " << D.pendingCount() << "\n";
        SlotNode* nf = D.nextFreeSlot();
        if (nf) cout << "Next free slot: " << nf->slotId << " [" << nf->startTime << "-" << nf->endTime << "]\n";
        else cout << "No free slots\n";
    }

    void servedVsPendingSummary() { cout << "Served: " << servedCount << " | Pending: " << pendingCountTotal << "\n"; }

    void topKFrequentPatients(int K) {
        vector<pair<int,int>> arr;
        for (auto &p : patients) arr.push_back({p.second.freq, p.first});
        sort(arr.begin(), arr.end(), greater<>());
        cout << "Top " << K << " frequent patients:\n";
        for (int i = 0; i < K && i < (int)arr.size(); ++i) {
            cout << "  PatientId " << arr[i].second << " freq " << arr[i].first << " name: " << patients[arr[i].second].name << "\n";
        }
    }

    void listDoctorSlots(int doctorId) {
        auto it = doctors.find(doctorId);
        if (it == doctors.end()) {
            cout << "Doctor not found\n";
            return;
        }
        it->second.printSlots();
    }

    void seedSampleData() {
        addDoctor(1, "Dr_Ahuja", "General", 5);
        addDoctor(2, "Dr_Mehta", "Cardio", 5);
        scheduleAddSlot(1, 101, "09:00", "09:15");
        scheduleAddSlot(1, 102, "09:15", "09:30");
        scheduleAddSlot(2, 201, "10:00", "10:15");
        patientUpsert(Patient{1,"Ananya",22,"No_history",0});
        patientUpsert(Patient{2,"Lakshita",19,"Allergy_pollen",0});
        patientUpsert(Patient{3,"Saieena",21,"Asthma",0});
    }
};

// ----------------------------- CLI -----------------------------
void printMenu() {
    cout << "\n=== Hospital Appointment & Triage System ===\n";
    cout << "1. Register/Update Patient\n2. Book Slot / Enqueue Routine\n3. Emergency In (Triage)\n4. Serve Next (doctor)\n5. Undo Last Action\n6. Reports\n7. List Doctor Slots\n8. Add Doctor\n9. Add Slot to Doctor\n0. Exit\nChoose option: ";
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    HospitalSystem H;
    H.seedSampleData();

    while (true) {
        printMenu();
        int opt; if (!(cin >> opt)) break;
        if (opt == 0) break;

        if (opt == 1) {
            Patient p; cout << "Enter patientId name age history (use _ for spaces): ";
            cin >> p.id >> p.name >> p.age >> p.history;
            H.patientUpsert(p); cout << "Registered/Updated patient " << p.name << " id " << p.id << "\n";
        }
        else if (opt == 2) {
            int pid, did; int slot = -1;
            cout << "Enter patientId doctorId (slotId or -1): "; cin >> pid >> did >> slot;
            int tok = H.enqueueRoutine(pid, did, slot);
            if (tok == -1) cout << "Booking failed (queue full/slot taken/invalid ids)\n"; else cout << "Booked tokenId: " << tok << "\n";
        }
        else if (opt == 3) {
            int pid, severity; cout << "Enter patientId severityScore (lower -> more urgent): "; cin >> pid >> severity;
            if (H.triageInsert(pid, severity)) cout << "Triage inserted\n"; else cout << "Triage failed (unknown patient)\n";
        }
        else if (opt == 4) {
            int did; cout << "Enter doctorId to serve next: "; cin >> did;
            Token served;
            if (!H.serveNext(did, served)) cout << "Nothing to serve for doctor " << did << "\n";
            else { cout << "Served tokenId " << served.tokenId << " patientId " << served.patientId << " type " << (served.type==EMERGENCY?"EMERGENCY":"ROUTINE") << "\n"; }
        }
        else if (opt == 5) {
            if (H.undoPop()) cout << "Undo successful\n"; else cout << "Nothing to undo or undo failed\n";
        }
        else if (opt == 6) {
            cout << "Reports menu:\n1. Per doctor summary\n2. Served vs pending\n3. Top-K frequent\nChoose: ";
            int r; cin >> r;
            if (r == 1) { int did; cout << "Enter doctorId: "; cin >> did; H.perDoctorReport(did); }
            else if (r == 2) H.servedVsPendingSummary();
            else if (r == 3) { int k; cin >> k; H.topKFrequentPatients(k); }
        }
        else if (opt == 7) {
            int did; cout << "Enter doctorId: "; cin >> did;
            H.listDoctorSlots(did);
        }
        else if (opt == 8) {
            int id; string name, spec; int cap; 
            cout << "Enter doctorId name specialization queueCapacity: ";
            cin >> id >> name >> spec >> cap;
            if (H.addDoctor(id, name, spec, cap)) cout << "Doctor added\n";
            else cout << "Failed (doctor already exists)\n";
        }
        else if (opt == 9) {
            int did, sid; string s, e;
            cout << "Enter doctorId slotId startTime endTime: ";
            cin >> did >> sid >> s >> e;
            if (H.scheduleAddSlot(did, sid, s, e)) cout << "Slot added\n";
            else cout << "Slot add failed (doctor not found)\n";
        }
    }

    return 0;
}
/*# Hospital Appointment & Triage System

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

GeeksforGeeks: Queues, Heaps, Hashing*/