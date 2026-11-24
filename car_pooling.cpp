#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <sstream>

using namespace std;

const int INF = numeric_limits<int>::max();

struct Edge {
    int to;
    int weight; // Distance in km
};

class CampusGraph {
private:
    int numVertices;
    vector<vector<Edge>> adj;
    vector<string> locationNames;

public:
    CampusGraph(int n) : numVertices(n), adj(n), locationNames(n) {}

    int getNumVertices() const { return numVertices; }

    void setLocationName(int id, const string& name) {
        if (id >= 0 && id < numVertices) {
            locationNames[id] = name;
        }
    }

    string getLocationName(int id) const {
        return (id >= 0 && id < numVertices) ? locationNames[id] : "Unknown";
    }

    void addRoad(int u, int v, int weight) {
        adj[u].push_back({v, weight});
        adj[v].push_back({u, weight});
    }

    void displayLocations() const {
        cout << "\n--- Campus & City Locations ---\n";
        for (int i = 0; i < numVertices; ++i) {
            cout << "  [" << i << "] " << locationNames[i] << "\n";
        }
        cout << "-------------------------------\n";
    }

    // Dijkstra's Algorithm: Returns {shortest_distance, path_of_nodes}
    pair<int, vector<int>> getShortestPath(int src, int dest) const {
        vector<int> dist(numVertices, INF);
        vector<int> parent(numVertices, -1);
        priority_queue<pair<int, int>, vector<pair<int, int>>, greater<pair<int, int>>> pq;

        dist[src] = 0;
        pq.push({0, src});

        while (!pq.empty()) {
            int d = pq.top().first;
            int u = pq.top().second;
            pq.pop();

            if (d > dist[u]) continue;
            if (u == dest) break;

            for (const auto& edge : adj[u]) {
                if (dist[u] != INF && dist[u] + edge.weight < dist[edge.to]) {
                    dist[edge.to] = dist[u] + edge.weight;
                    parent[edge.to] = u;
                    pq.push({dist[edge.to], edge.to});
                }
            }
        }

        vector<int> path;
        if (dist[dest] == INF) return {INF, path};

        for (int curr = dest; curr != -1; curr = parent[curr]) {
            path.push_back(curr);
        }
        reverse(path.begin(), path.end());
        return {dist[dest], path};
    }

    int getShortestDistance(int src, int dest) const {
        return getShortestPath(src, dest).first;
    }
};

// Time helpers
int parseTimeToMinutes(const string& timeStr) {
    int hours = 0, minutes = 0;
    char colon;
    stringstream ss(timeStr);
    if (ss >> hours >> colon >> minutes && colon == ':' && hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60) {
        return hours * 60 + minutes;
    }
    return -1;
}

string formatMinutesToTime(int totalMinutes) {
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    ostringstream oss;
    oss << setw(2) << setfill('0') << h << ":" << setw(2) << setfill('0') << m;
    return oss.str();
}

struct User {
    int id;
    string name;
    string role;
    int originNode;
    int destNode;
    int departureTime;

    User(int id, string name, string role, int origin, int dest, int time)
        : id(id), name(name), role(role), originNode(origin), destNode(dest), departureTime(time) {}
};

struct Driver : public User {
    int capacity;
    int maxDetour;
    vector<int> assignedRiders;
    vector<int> optimalPickupOrder;
    int currentTotalDetour;

    Driver(int id, string name, string role, int origin, int dest, int time, int cap, int detour)
        : User(id, name, role, origin, dest, time), capacity(cap), maxDetour(detour), currentTotalDetour(0) {}
};

struct Rider : public User {
    bool isMatched;
    int assignedDriverId;
    double fareShare; // Proportional fuel cost share in INR

    Rider(int id, string name, string role, int origin, int dest, int time)
        : User(id, name, role, origin, dest, time), isMatched(false), assignedDriverId(-1), fareShare(0.0) {}
};

class CarpoolEngine {
private:
    CampusGraph& graph;
    vector<Driver> drivers;
    vector<Rider> riders;
    int nextDriverId = 101;
    int nextRiderId = 201;
    double fuelCostPerKm = 8.0; // Default ₹8.00 per km

    const Rider* getRiderById(int riderId) const {
        for (const auto& r : riders) {
            if (r.id == riderId) return &r;
        }
        return nullptr;
    }

    Rider* getMutableRiderById(int riderId) {
        for (auto& r : riders) {
            if (r.id == riderId) return &r;
        }
        return nullptr;
    }

    // TSP Solver using std::next_permutation: O(k! * k)
    pair<int, vector<int>> solveOptimalPickupTSP(const Driver& d, const vector<int>& candidateRiderIds) const {
        int directDist = graph.getShortestDistance(d.originNode, d.destNode);
        if (directDist == INF) return {INF, {}};

        vector<int> perm = candidateRiderIds;
        sort(perm.begin(), perm.end());

        int minTotalRouteDist = INF;
        vector<int> bestOrder;

        do {
            int currentRouteDist = 0;
            int currentLocation = d.originNode;
            bool valid = true;

            for (int rId : perm) {
                const Rider* r = getRiderById(rId);
                if (!r) { valid = false; break; }

                int legDist = graph.getShortestDistance(currentLocation, r->originNode);
                if (legDist == INF) { valid = false; break; }

                currentRouteDist += legDist;
                currentLocation = r->originNode;
            }

            if (valid) {
                int finalLeg = graph.getShortestDistance(currentLocation, d.destNode);
                if (finalLeg == INF) {
                    valid = false;
                } else {
                    currentRouteDist += finalLeg;
                }
            }

            if (valid && currentRouteDist < minTotalRouteDist) {
                minTotalRouteDist = currentRouteDist;
                bestOrder = perm;
            }

        } while (next_permutation(perm.begin(), perm.end()));

        if (minTotalRouteDist == INF) return {INF, {}};
        return {minTotalRouteDist - directDist, bestOrder};
    }

    // Segment-by-Segment Proportional Cost Splitting: O(k)
    // Returns driver's own fuel share after updating every matched rider's fareShare
    double computeFuelSplit(const Driver& d) {
        int directDist = graph.getShortestDistance(d.originNode, d.destNode);
        if (d.optimalPickupOrder.empty()) {
            return directDist * fuelCostPerKm;
        }

        // Build ordered stop nodes: [DriverStart, Pickup_1, ..., Pickup_k, Destination]
        vector<int> stops;
        stops.push_back(d.originNode);
        for (int rId : d.optimalPickupOrder) {
            const Rider* r = getRiderById(rId);
            if (r) stops.push_back(r->originNode);
        }
        stops.push_back(d.destNode);

        // Reset rider fares before recalculating across all legs
        for (int rId : d.optimalPickupOrder) {
            Rider* r = getMutableRiderById(rId);
            if (r) r->fareShare = 0.0;
        }

        double driverShare = 0.0;
        int numLegs = (int)stops.size() - 1;

        for (int leg = 0; leg < numLegs; ++leg) {
            int legDist = graph.getShortestDistance(stops[leg], stops[leg + 1]);
            double legCost = legDist * fuelCostPerKm;

            // Occupants on leg i: Driver + first 'i' picked-up riders = (leg + 1) people
            int occupants = leg + 1;
            double perPersonCost = legCost / occupants;

            driverShare += perPersonCost;

            // Add this leg's per-person cost to every rider already onboard (indices 0 .. leg - 1)
            for (int rIdx = 0; rIdx < leg; ++rIdx) {
                Rider* r = getMutableRiderById(d.optimalPickupOrder[rIdx]);
                if (r) r->fareShare += perPersonCost;
            }
        }

        return driverShare;
    }

public:
    CarpoolEngine(CampusGraph& g) : graph(g) {}

    void setFuelRate(double rate) { fuelCostPerKm = rate; }
    double getFuelRate() const { return fuelCostPerKm; }

    void addDriver(string name, string role, int origin, int dest, int time, int cap, int detour) {
        drivers.push_back(Driver(nextDriverId++, name, role, origin, dest, time, cap, detour));
    }

    void addRider(string name, string role, int origin, int dest, int time) {
        riders.push_back(Rider(nextRiderId++, name, role, origin, dest, time));
    }

    void matchRiders() {
        cout << "\n======================================================\n";
        cout << "   RUNNING CARPOOL MATCHING + TSP ROUTE OPTIMIZER\n";
        cout << "======================================================\n";

        int newMatches = 0;
        for (auto& rider : riders) {
            if (rider.isMatched) continue;

            int bestDriverIdx = -1;
            int bestMarginalDetour = INF;
            int bestNewTotalDetour = INF;
            vector<int> bestNewPickupOrder;

            for (int i = 0; i < (int)drivers.size(); ++i) {
                Driver& d = drivers[i];

                if ((int)d.assignedRiders.size() >= d.capacity) continue;
                if (d.destNode != rider.destNode) continue;
                if (abs(d.departureTime - rider.departureTime) > 15) continue;

                vector<int> testRiders = d.assignedRiders;
                testRiders.push_back(rider.id);

                auto tspResult = solveOptimalPickupTSP(d, testRiders);
                int newTotalDetour = tspResult.first;

                if (newTotalDetour == INF) continue;

                if (newTotalDetour <= d.maxDetour) {
                    int marginalDetour = newTotalDetour - d.currentTotalDetour;
                    if (marginalDetour < bestMarginalDetour) {
                        bestMarginalDetour = marginalDetour;
                        bestNewTotalDetour = newTotalDetour;
                        bestNewPickupOrder = tspResult.second;
                        bestDriverIdx = i;
                    }
                }
            }

            if (bestDriverIdx != -1) {
                Driver& chosenDriver = drivers[bestDriverIdx];
                chosenDriver.assignedRiders.push_back(rider.id);
                chosenDriver.optimalPickupOrder = bestNewPickupOrder;
                chosenDriver.currentTotalDetour = bestNewTotalDetour;

                rider.isMatched = true;
                rider.assignedDriverId = chosenDriver.id;
                newMatches++;

                cout << "[MATCHED] Rider: " << rider.name << " (" << rider.role << ") at "
                     << graph.getLocationName(rider.originNode)
                     << "\n       -> Assigned Driver: " << chosenDriver.name
                     << " | Added Detour: +" << bestMarginalDetour << " km"
                     << " (Total Trip Detour: " << chosenDriver.currentTotalDetour << "/" << chosenDriver.maxDetour << " km)\n\n";
            } else {
                cout << "[PENDING] Rider: " << rider.name << " (" << rider.role
                     << ") - No compatible driver within detour/seat limits.\n\n";
            }
        }

        // Recompute proportional fuel shares across all drivers
        for (const auto& d : drivers) {
            computeFuelSplit(d);
        }

        if (newMatches == 0) {
            cout << "No new matches made during this run.\n";
        }
    }

    void printManifest() {
        cout << fixed << setprecision(2);
        cout << "\n======================================================\n";
        cout << "   OPTIMIZED CARPOOL MANIFEST & FUEL COST SPLITTING\n";
        cout << "   (Current Fuel Rate: Rs. " << fuelCostPerKm << " / km)\n";
        cout << "======================================================\n";

        for (const auto& d : drivers) {
            int directDist = graph.getShortestDistance(d.originNode, d.destNode);
            int totalDist = directDist + d.currentTotalDetour;
            double totalTripCost = totalDist * fuelCostPerKm;
            double soloTripCost = directDist * fuelCostPerKm;
            double driverShare = computeFuelSplit(d);

            cout << "Driver #" << d.id << ": " << d.name << " [" << d.role << "] @ "
                 << formatMinutesToTime(d.departureTime) << "\n"
                 << "   Seats: " << d.assignedRiders.size() << "/" << d.capacity
                 << " | Direct: " << directDist << " km | Total Route: " << totalDist << " km"
                 << " (Detour: " << d.currentTotalDetour << "/" << d.maxDetour << " km)\n"
                 << "   Total Trip Fuel Cost: Rs. " << totalTripCost
                 << " | Driver's Share: Rs. " << driverShare
                 << " (Saved Rs. " << max(0.0, soloTripCost - driverShare) << " vs solo)\n";

            if (d.optimalPickupOrder.empty()) {
                cout << "   Route: " << graph.getLocationName(d.originNode)
                     << " -> " << graph.getLocationName(d.destNode) << " (No passengers)\n";
            } else {
                cout << "   Optimal Pickup Sequence & Fair Cost Breakdown:\n"
                     << "      Start : " << graph.getLocationName(d.originNode) << "\n";
                for (size_t i = 0; i < d.optimalPickupOrder.size(); ++i) {
                    const Rider* r = getRiderById(d.optimalPickupOrder[i]);
                    if (r) {
                        int riderDirect = graph.getShortestDistance(r->originNode, r->destNode);
                        cout << "      Stop " << (i + 1) << ": Pick up " << r->name
                             << " at [" << graph.getLocationName(r->originNode) << "]"
                             << " -> Pays Rs. " << r->fareShare
                             << " (Solo cost would be Rs. " << (riderDirect * fuelCostPerKm) << ")\n";
                    }
                }
                cout << "      End   : " << graph.getLocationName(d.destNode) << "\n";
            }
            cout << "------------------------------------------------------\n";
        }

        cout << "\n=== RIDER STATUS & FARE SUMMARY ===\n";
        for (const auto& r : riders) {
            cout << "Rider #" << r.id << ": " << r.name << " [" << r.role << "] @ "
                 << formatMinutesToTime(r.departureTime) << " | Pickup: "
                 << graph.getLocationName(r.originNode) << " -> ";
            if (r.isMatched) {
                cout << "MATCHED (Driver #" << r.assignedDriverId << ") | Fare: Rs. " << r.fareShare << "\n";
            } else {
                cout << "UNMATCHED\n";
            }
        }
    }
};

// Input validation helpers
int getValidInt(const string& prompt, int minVal, int maxVal) {
    int val;
    while (true) {
        cout << prompt;
        if (cin >> val && val >= minVal && val <= maxVal) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return val;
        }
        cout << "Invalid input. Enter a number between " << minVal << " and " << maxVal << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

double getValidDouble(const string& prompt, double minVal, double maxVal) {
    double val;
    while (true) {
        cout << prompt;
        if (cin >> val && val >= minVal && val <= maxVal) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            return val;
        }
        cout << "Invalid input. Enter a value between " << minVal << " and " << maxVal << ".\n";
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
}

string getValidTime(const string& prompt) {
    string t;
    while (true) {
        cout << prompt;
        getline(cin, t);
        if (parseTimeToMinutes(t) != -1) return t;
        cout << "Invalid format. Please use 24-hour HH:MM (e.g., 08:30 or 14:15).\n";
    }
}

int main() {
    CampusGraph campusMap(6);
    campusMap.setLocationName(0, "Sector 14 Residential");
    campusMap.setLocationName(1, "City Metro Station");
    campusMap.setLocationName(2, "Highway Toll Plaza");
    campusMap.setLocationName(3, "Faculty Housing Colony");
    campusMap.setLocationName(4, "University Main Gate");
    campusMap.setLocationName(5, "Academic Block A");

    campusMap.addRoad(0, 1, 4);
    campusMap.addRoad(1, 2, 5);
    campusMap.addRoad(0, 3, 6);
    campusMap.addRoad(3, 4, 3);
    campusMap.addRoad(2, 4, 4);
    campusMap.addRoad(4, 5, 2);

    CarpoolEngine engine(campusMap);

    // Pre-loaded sample scenario
    engine.addDriver("Dr. Sharma", "Faculty", 0, 5, parseTimeToMinutes("08:30"), 3, 6);
    engine.addDriver("Rohan", "Student", 2, 5, parseTimeToMinutes("08:40"), 2, 3);

    engine.addRider("Ananya", "Student", 3, 5, parseTimeToMinutes("08:35"));
    engine.addRider("Vikram", "Staff", 1, 5, parseTimeToMinutes("08:30"));

    int choice;
    while (true) {
        cout << "\n======================================================\n";
        cout << "   UNIVERSITY CARPOOLING SYSTEM (DIJKSTRA + TSP CLI)\n";
        cout << "======================================================\n";
        cout << "1. View Campus & City Locations\n";
        cout << "2. Find Shortest Path Between Two Locations\n";
        cout << "3. Register a New Driver\n";
        cout << "4. Register a New Rider\n";
        cout << "5. Run Carpool Matching & TSP Route Optimizer\n";
        cout << "6. View Optimized Manifest & Fair Cost Split\n";
        cout << "7. Update Fuel Rate (Current: Rs. " << fixed << setprecision(2) << engine.getFuelRate() << "/km)\n";
        cout << "8. Exit\n";
        choice = getValidInt("Enter your choice (1-8): ", 1, 8);

        if (choice == 1) {
            campusMap.displayLocations();

        } else if (choice == 2) {
            campusMap.displayLocations();
            int n = campusMap.getNumVertices() - 1;
            int src = getValidInt("Enter Source Location ID: ", 0, n);
            int dest = getValidInt("Enter Destination Location ID: ", 0, n);

            auto result = campusMap.getShortestPath(src, dest);
            if (result.first == INF) {
                cout << "No path exists between these locations.\n";
            } else {
                cout << "\nShortest Distance: " << result.first << " km\nRoute: ";
                for (size_t i = 0; i < result.second.size(); ++i) {
                    cout << campusMap.getLocationName(result.second[i]);
                    if (i + 1 < result.second.size()) cout << " -> ";
                }
                cout << "\n";
            }

        } else if (choice == 3) {
            campusMap.displayLocations();
            int n = campusMap.getNumVertices() - 1;
            string name, role;
            cout << "Enter Driver Name: ";
            getline(cin, name);
            cout << "Enter Role (Student/Staff/Faculty): ";
            getline(cin, role);
            int origin = getValidInt("Enter Starting Location ID: ", 0, n);
            int dest = getValidInt("Enter Destination Location ID: ", 0, n);
            string timeStr = getValidTime("Enter Departure Time (HH:MM, 24-hr): ");
            int cap = getValidInt("Enter Available Passenger Seats (1-6): ", 1, 6);
            int detour = getValidInt("Enter Max Detour Tolerance in km (0-15): ", 0, 15);

            engine.addDriver(name, role, origin, dest, parseTimeToMinutes(timeStr), cap, detour);
            cout << "Driver registered successfully!\n";

        } else if (choice == 4) {
            campusMap.displayLocations();
            int n = campusMap.getNumVertices() - 1;
            string name, role;
            cout << "Enter Rider Name: ";
            getline(cin, name);
            cout << "Enter Role (Student/Staff/Faculty): ";
            getline(cin, role);
            int origin = getValidInt("Enter Pickup Location ID: ", 0, n);
            int dest = getValidInt("Enter Destination Location ID: ", 0, n);
            string timeStr = getValidTime("Enter Preferred Departure Time (HH:MM, 24-hr): ");

            engine.addRider(name, role, origin, dest, parseTimeToMinutes(timeStr));
            cout << "Rider registered successfully!\n";

        } else if (choice == 5) {
            engine.matchRiders();

        } else if (choice == 6) {
            engine.printManifest();

        } else if (choice == 7) {
            double rate = getValidDouble("Enter new fuel rate per km (Rs. 1.0 - 100.0): ", 1.0, 100.0);
            engine.setFuelRate(rate);
            cout << "Fuel rate updated to Rs. " << fixed << setprecision(2) << rate << "/km.\n";

        } else if (choice == 8) {
            cout << "Exiting University Carpooling System. Goodbye!\n";
            break;
        }
    }

    return 0;
}