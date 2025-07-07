import math
import itertools

# --- Distance between 2 points ---
def distance(p1, p2):
    return math.hypot(p1[0]-p2[0], p1[1]-p2[1])

# --- Calculate total route distance for 1 UGV ---
def total_time(start_pos, spot_order):
    if not spot_order:
        return 0
    total = distance(start_pos, spot_order[0])
    for i in range(1, len(spot_order)):
        total += distance(spot_order[i-1], spot_order[i])
    return total

# --- Generate all possible assignments of spots to UGVs ---
def generate_assignments(spots, n_ugvs):
    from collections import defaultdict

    ugv_ids = list(range(n_ugvs))
    all_assignments = []

    # Generate all possible distributions
    for combo in itertools.product(ugv_ids, repeat=len(spots)):
        assignment = defaultdict(list)
        for idx, ugv_id in enumerate(combo):
            assignment[ugv_id].append(spots[idx])
        all_assignments.append(assignment)

    return all_assignments

# --- Main optimization function ---
def assign_spots_optimally(ugv_positions, spots):
    n_ugvs = len(ugv_positions)
    best_assignment = None
    best_max_time = float('inf')

    all_assignments = generate_assignments(spots, n_ugvs)

    for assignment in all_assignments:
        ugv_times = []
        for ugv_id in range(n_ugvs):
            route = assignment.get(ugv_id, [])
            ugv_times.append(total_time(ugv_positions[ugv_id], route))
        max_time = max(ugv_times)

        if max_time < best_max_time:
            best_max_time = max_time
            best_assignment = assignment

    return best_assignment

# --- Run the program ---
def main():
    print("Enter number of UGVs:")
    n_ugvs = int(input())

    ugv_positions = []
    for i in range(n_ugvs):
        print(f"Enter position of UGV {i+1} as x y:")
        x, y = map(float, input().split())
        ugv_positions.append((x, y))

    print("Enter number of spots:")
    n_spots = int(input())

    spots = []
    for i in range(n_spots):
        print(f"Enter position of Spot {i+1} as x y:")
        x, y = map(float, input().split())
        spots.append((x, y))

    best_assignment = assign_spots_optimally(ugv_positions, spots)

    print("\n--- Spot Assignments (Time-balanced) ---")
    for ugv_id in range(n_ugvs):
        assigned = best_assignment.get(ugv_id, [])
        print(f"UGV {ugv_id+1} -> {assigned}")

if __name__ == "__main__":
    main()
