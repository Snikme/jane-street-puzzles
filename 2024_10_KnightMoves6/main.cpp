//  Created by Kirill Sandor

#include <iostream>
#include <cstdint>
#include <iomanip>
#include <thread>
#include <mutex>

using namespace std;

// ************************************************************************************

const vector<vector<int8_t>> BOARD_LAYOUT = {
    {'a', 'b', 'b', 'c', 'c', 'c'},
    {'a', 'b', 'b', 'c', 'c', 'c'},
    {'a', 'a', 'b', 'b', 'c', 'c'},
    {'a', 'a', 'b', 'b', 'c', 'c'},
    {'a', 'a', 'a', 'b', 'b', 'c'},
    {'a', 'a', 'a', 'b', 'b', 'c'},
};

const int8_t BOARD_SIZE = BOARD_LAYOUT.size();
const size_t TARGET_SCORE = 2024;
const int8_t DEFAULT_CELL_VALUE = -1;

// trying to target A B C as low as possible (plus all permutations)
vector<int8_t> TARGET_ABC = {1,2,3};

const pair<int8_t, int8_t> TRIP_A1F6_START = {5,0};
const pair<int8_t, int8_t> TRIP_A1F6_FINISH = {0,5};

const pair<int8_t, int8_t> TRIP_A6F1_START = {0,0};
const pair<int8_t, int8_t> TRIP_A6F1_FINISH = {5,5};

/*
 We need an even number of knight's moves, as diagonal corner cells share the same color,
 and a knight always moves to a square of the opposite color from where it currently is.
 We need at least 8 moves, as this is the smallest number that might score the target points.
 This can be verified by a approximate upper bound check, assuming all moves result in
 the multiplication of the greatest target value we picked, which is 3.
 */
const int8_t TRIP_LENGTH_MIN = 8;
const int8_t TRIP_LENGTH_MAX = BOARD_SIZE * BOARD_SIZE - 1;

// ************************************************************************************

unordered_map<size_t, vector<pair<int8_t, int8_t>>> valid_trips;

mutex valid_trips_mutex;
mutex print_mutex;

// ************************************************************************************

void printBoard(const vector<vector<int8_t>>& board) {
    lock_guard<mutex> lock(print_mutex);
    const int8_t w = 3;
    for(const vector<int8_t>& row : board) {
        for(const int8_t& cell : row) {
            std::cout << std::right << std::setw(w);
            if(cell == -1) {
                cout << "__";
            } else {
                cout << static_cast<int>(cell);
            }
        }
        
        cout << endl;
    }
    cout << endl;
}

void printTrip(const vector<pair<int8_t, int8_t>>& trip) {
    lock_guard<mutex> lock(print_mutex);
    int8_t trip_length = trip.size();
    if(trip_length > 0) {
        for(int8_t i = 0; i < trip_length - 1; ++i) cout << char(trip[i].second + 'a') << int(BOARD_SIZE - trip[i].first) << ",";
        cout << char(trip.back().second + 'a') << int(BOARD_SIZE - trip.back().first);
    }
}

// ************************************************************************************

struct CandidateBoard {
public:
    CandidateBoard(int8_t& a, int8_t& b, int8_t& c): a(a), b(b), c(c) {
        setMapKey();
        fillLayout();
    }
    
    bool isValidTrip(const vector<pair<int8_t, int8_t>>& trip_tracker, const int8_t& trip_length) {
        size_t score = layout[trip_tracker[0].first][trip_tracker[0].second];
        
        for(int8_t i = 1; i <= trip_length; ++i) {
            if(layout[trip_tracker[i-1].first][trip_tracker[i-1].second] == layout[trip_tracker[i].first][trip_tracker[i].second]) {
                score += layout[trip_tracker[i].first][trip_tracker[i].second];
            } else {
                score *= layout[trip_tracker[i].first][trip_tracker[i].second];
            }
            
            if(score > TARGET_SCORE) return false;
        }
        
        return score == TARGET_SCORE;
    }
    
    // ************************************************************************************
    
    size_t getMapKey() {
        return map_key;
    }
    
    int8_t getA() {
        return a;
    }
    
    int8_t getB() {
        return b;
    }
    
    int8_t getC() {
        return c;
    }
private:
    int8_t a, b, c;
    size_t map_key;
    
    vector<vector<int8_t>> layout = BOARD_LAYOUT;
    
    // good enough as our target is to find ABC as low as possible
    void setMapKey() {
        map_key = a * 100 + b * 10 + c;
    }
    
    void fillLayout() {
        for(vector<int8_t>& row : layout) {
            for(int8_t& cell : row) {
                switch(cell) {
                    case 'a': cell = a; break;
                    case 'b': cell = b; break;
                    case 'c': cell = c; break;
                }
            }
        }
    }
};

// ************************************************************************************

void printResult(const int8_t& a, const int8_t& b, const int8_t& c, vector<pair<int8_t, int8_t>>& trip1, vector<pair<int8_t, int8_t>>& trip2) {
    cout << "I found it!" << endl;
    cout << static_cast<int>(a) << "," << static_cast<int>(b) << "," << static_cast<int>(c) << ",";
    
    if(trip1[0] == TRIP_A6F1_START) swap(trip1, trip2);
    
    printTrip(trip1);
    cout << ",";
    printTrip(trip2);
    cout << endl;
}

// ************************************************************************************

struct TripsFinder {
public:
    TripsFinder(const pair<int8_t, int8_t>& start, const pair<int8_t, int8_t>& finish, const vector<CandidateBoard>& candidate_boards) {
        start_i = start.first;
        start_j = start.second;
        
        finish_i = finish.first;
        finish_j = finish.second;
        
        this->candidate_boards = candidate_boards;
    };
    
    void run() {
        while(trip_length <= TRIP_LENGTH_MAX) {
            board = vector<vector<int8_t>>(BOARD_SIZE, vector<int8_t>(BOARD_SIZE, DEFAULT_CELL_VALUE));
            
            trip_tracker = vector<pair<int8_t, int8_t>>();
            trip_tracker.reserve(trip_length + 1);
            
            board[start_i][start_j] = move_count;
            trip_tracker.emplace_back(start_i, start_j);
            
            move(start_i, start_j);
            trip_length++;
        }
    }
private:
    const vector<pair<int8_t, int8_t>> KNIGHT_MOVES = {
        {2, 1},
        {1, 2},
        {-1, 2},
        {-2, 1},
        {-2, -1},
        {-1, -2},
        {1, -2},
        {2, -1},
    };
    
    int8_t start_i, start_j, finish_i, finish_j;
    int8_t trip_length = TRIP_LENGTH_MIN;
    int8_t move_count = 0;
    
    vector<vector<int8_t>> board;
    vector<pair<int8_t, int8_t>> trip_tracker;
    
    // we have this for each instance as we remove candidates from the list once found to avoid unnecessary score calculations for future trips
    vector<CandidateBoard> candidate_boards;
    
    // ************************************************************************************
    
    bool isValidMove(int8_t& i, int8_t& j) {
        return i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE && board[i][j] == DEFAULT_CELL_VALUE;
    }
    
    void tryCandidateBoards() {
        for(auto it = candidate_boards.begin(); it != candidate_boards.end();) {
            if(it->isValidTrip(trip_tracker, trip_length)) {
                lock_guard<mutex> lock(valid_trips_mutex);
                
                if(valid_trips.count(it->getMapKey())) {
                    printResult(it->getA(), it->getB(), it->getC(), valid_trips[it->getMapKey()], trip_tracker);
                    exit(1);
                }
                
                valid_trips.emplace(it->getMapKey(), trip_tracker);
                it = candidate_boards.erase(it);
            } else {
                it++;
            }
        }
    }
    
    void move(int8_t& i, int8_t& j) {
        move_count++;
        
        if(move_count > trip_length) {
            if(i == finish_i && j == finish_j) {
                tryCandidateBoards();
            }
            
            move_count--;
            return;
        }
        
        for(auto& [di, dj] : KNIGHT_MOVES) {
            int8_t next_i = i + di, next_j = j + dj;
            
            if(isValidMove(next_i, next_j)) {
                board[next_i][next_j] = move_count;
                trip_tracker.emplace_back(next_i, next_j);
                
                move(next_i, next_j);
                
                board[next_i][next_j] = DEFAULT_CELL_VALUE;
                trip_tracker.pop_back();
            }
        }
        
        move_count--;
        return;
    }
};

// ************************************************************************************

vector<CandidateBoard> generateCandidateBoards() {
    vector<CandidateBoard> r;
    do {
        r.emplace_back(TARGET_ABC[0], TARGET_ABC[1], TARGET_ABC[2]);
    } while (std::next_permutation(TARGET_ABC.begin(), TARGET_ABC.end()));
    
    return r;
}

// ************************************************************************************

int main(int argc, const char * argv[]) {
    
    vector<CandidateBoard> candidate_boards = generateCandidateBoards();
    
    TripsFinder tripFinder1 = TripsFinder(TRIP_A6F1_START, TRIP_A6F1_FINISH, candidate_boards);
    TripsFinder tripFinder2 = TripsFinder(TRIP_A1F6_START, TRIP_A1F6_FINISH, candidate_boards);
    
    thread t1([&tripFinder1]() { tripFinder1.run(); });
    thread t2([&tripFinder2]() { tripFinder2.run(); });
    
    t1.join();
    t2.join();

    cout << "Trips not found :(" << endl;
    return 0;
}
