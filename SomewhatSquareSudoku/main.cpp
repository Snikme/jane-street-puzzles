//  Created by Kirill Sandor

#include <iostream>
#include <cinttypes>
#include <unordered_set>
#include <map>
#include <thread>
#include <mutex>

using namespace std;

// ************************************************************************************

class SomewhatSquareSudoku {
public:
    
    // solve the puzzle and returns the number formed by the middle row in the completed grid.
    int solve() {
        
        // all possible remaining digits for second row number (already contain 2 and 5)
        vector<uint8_t> digits = {0, 1, 3, 4, 6, 7, 8, 9}, subset;
        uint8_t placements_count = 7, digits_size = 8;
        
        subset.reserve(placements_count);

        vector<bool> mask(digits.size(), false);
        fill(mask.begin(), mask.begin() + placements_count, true);
        
        // sorted list of divisors with relation to its multiples (possible second row numbers)
        map<int, vector<int>, greater<int>> divisors;

        do {
            // all numbers must contain 0
            if(mask[0] == false) continue;
            
            subset.clear();
            for (size_t i = 0; i < digits_size; ++i) if (mask[i]) subset.push_back(digits[i]);

            do {
                if(subset[2] == 0) continue;
                
                int num = subsetToInt(subset, placements_count);
                findDivisors(num, divisors);
            } while(next_permutation(subset.begin(), subset.end()));
            
        } while(prev_permutation(mask.begin(), mask.end()));
        
        // ************************************************************************************
        
        // constraints on sudoku possible numbers
        int min_number = 12'345'678, max_number = 987'654'321;

        for(auto& [k, multiples] : divisors) {
            
            // to be able to bind as a reference in threads
            int divisor = k;
            
            for(int& multiple : multiples) {
                size_t valid_candidates_len9 = 0, valid_candidates_len8 = 0;

                vector<vector<vector<uint8_t>>> valid_candidates(9);
                
                // ************************************************************************************
                
                mutex mtx;
            
                thread tUpward(bind(findCandidatesUpward, ref(multiple), ref(divisor), ref(max_number), ref(valid_candidates), ref(valid_candidates_len9), ref(valid_candidates_len8), ref(mtx)));
                thread tDownward(bind(findCandidatesDownward, ref(multiple), ref(divisor), ref(min_number), ref(valid_candidates), ref(valid_candidates_len9), ref(valid_candidates_len8), ref(mtx)));
                
                tUpward.join();
                tDownward.join();
                
                // ************************************************************************************
                
                if(multiple >= 100'000'000) {
                    if(valid_candidates_len9 < 7 || valid_candidates_len8 < 1) continue;
                } else {
                    if(valid_candidates_len9 < 8) continue;
                }
                
                if(any_of(valid_candidates.begin(), valid_candidates.end(), [](const vector<vector<uint8_t>>& a){return a.empty();})) continue;
                
                // ************************************************************************************
                
                // find all the possible combinations of sudoku grids and validate them
                vector<uint8_t> indices(9, 0);
                int combinations = 1;
                
                for (const auto& row : valid_candidates) combinations *= row.size();
                
                for (int i = 0; i < combinations; i++) {
                    vector<vector<uint8_t>> sudoku(9);
                    
                    for (int j = 0; j < 9; j++) sudoku[j] = valid_candidates[j][indices[j]];
                    
                    if(isValidSudoku(sudoku)) {
                        int answer = 0, order = 1;
                        for(uint8_t el : sudoku[4]) {
                            answer += el * order;
                            order *= 10;
                        }
                        
                        cout << "Answer to the puzzle: " << answer << endl;
                        
//                        printSudoku(sudoku);
                        return 1;
                    }
                    
                    for (int j = 8; j >= 0; j--) {
                        if (++indices[j] < valid_candidates[j].size()) break;
                        indices[j] = 0;
                    }
                }
            }
        }
        
        return 0;
    }
    
private:
    int subsetToInt(vector<uint8_t>& subset, uint8_t& placements_count) {
        int num = 0;

        for(uint8_t i = 0; i < 4; ++i) num = num * 10 + subset[i];
        
        // we know the position of 2 in second row number
        num = num * 10 + 2;

        for(uint8_t i = 4; i < placements_count; ++i) num = num * 10 + subset[i];

        // the last position is 5 in second row number
        return num * 10 + 5;
    }
    
    void findDivisors(int& num, map<int, vector<int>, greater<int>>& divisors) {
        int max_i = sqrt(num);

        for(int i = 1; i <= max_i; ++i) {
            if(num % i == 0) {
                int div = num / i;
                
                // one of the sudoku numbers is 8 digits length as we have 0 in it, so divisor can not be 9 digits
                if(div > 99'999'999 || i > 99'999'999) continue;
                
                int dr = div % 10;
                
                // divisor must end with 1,3,7,9 as we need 9 sudoku numbers be odd and even
                if(dr == 1 || dr == 3 || dr == 7 || dr == 9) divisors[div].push_back(num);
                if(i == 1  || i == 3  || i == 7  || i == 9 ) divisors[i].push_back(num);
            }
        }
    }
    
    static void findCandidatesUpward(int& multiple, int& divisor, int& max_number, vector<vector<vector<uint8_t>>>& valid_candidates, size_t& valid_candidates_len9, size_t&valid_candidates_len8, mutex& mtx) {
        int from = multiple + divisor;
        for(int candidate = from; candidate <= max_number; candidate += divisor) {
            if(validateCandidateAndAdd(multiple, candidate, valid_candidates, mtx)) {
                lock_guard<mutex> lock(mtx);
                candidate >= 100'000'000 ? valid_candidates_len9++ : valid_candidates_len8++;
            }
        }
    }
    
    static void findCandidatesDownward(int& multiple, int& divisor, int& min_number, vector<vector<vector<uint8_t>>>& valid_candidates, size_t& valid_candidates_len9, size_t&valid_candidates_len8, mutex& mtx) {
        int from = multiple - divisor;
        for(int candidate = from; candidate >= min_number; candidate -= divisor) {
            if(validateCandidateAndAdd(multiple, candidate, valid_candidates, mtx)) {
                lock_guard<mutex> lock(mtx);
                candidate >= 100'000'000 ? valid_candidates_len9++ : valid_candidates_len8++;
            }
        }
    }
    
    static bool validateCandidateAndAdd(int& multiple, int& candidate, vector<vector<vector<uint8_t>>>& valid_candidates, mutex& mtx) {
        // both nunbers are 8 digits in length which can not be in sudoku grid
        if(multiple < 100'000'000 && candidate < 100'000'000) return false;
        
        // ************************************************************************************
        
        int mask_multiple = 0, mask_candidate = 0;
        vector<uint8_t> digits_multiple, digits_candidate;

        digits_multiple.reserve(9);
        digits_candidate.reserve(9);
        
        // ************************************************************************************
        
        if(!extractDigits(candidate, mask_candidate, digits_candidate, true)) return false;
        extractDigits(multiple, mask_multiple, digits_multiple);
        
        // ************************************************************************************
        
        if(mask_multiple != mask_candidate) return false;
        for(int i = 0; i < 9; ++i) if(digits_multiple[i] == digits_candidate[i]) return false;
        
        // ************************************************************************************
        
        uint8_t row_index = 4;
        
        // validation rules for the digits we have in the grid
        // 1, 3, 6, 8 rows
        if(digits_candidate[1] == 2 || digits_candidate[3] == 2 || digits_candidate[5] == 2 || digits_candidate[7] == 2) {
            if(digits_candidate[2] == 5 || digits_candidate[4] == 0 || digits_candidate[6] == 0) return false;
            
            if(digits_candidate[1] == 2) row_index = 0;
            if(digits_candidate[3] == 2) row_index = 7;
            if(digits_candidate[5] == 2) row_index = 5;
            if(digits_candidate[7] == 2) row_index = 2;
        }
        
        // 9 row
        if(digits_candidate[2] == 5) {
            if(digits_candidate[1] == 2 || digits_candidate[3] == 2 || digits_candidate[5] == 2 || digits_candidate[7] == 2 || digits_candidate[4] == 0 || digits_candidate[6] == 0) return false;
            
            row_index = 8;
        }

        // 4 and 7 rows
        if(digits_candidate[4] == 0 || digits_candidate[6] == 0) {
            if(digits_candidate[1] == 2 || digits_candidate[3] == 2 || digits_candidate[5] == 2 || digits_candidate[7] == 2 || digits_candidate[2] == 5) return false;
            
            if(digits_candidate[4] == 0) row_index = 6;
            if(digits_candidate[6] == 0) row_index = 3;
        }
        
        // ************************************************************************************
        
        lock_guard<mutex> lock(mtx);
        if(valid_candidates[1].empty()) valid_candidates[1].push_back(digits_multiple);
        
        valid_candidates[row_index].push_back(digits_candidate);
        return true;
    }
    
    static bool extractDigits(int number, int& mask, vector<uint8_t>& digits, bool check_uniqueness = false) {
        while(number > 0) {
            int digit = number % 10;
            
            // number has non unique digits
            if(check_uniqueness) if(mask & (1 << digit)) return false;

            mask |= (1 << digit);
            digits.push_back(digit);

            number /= 10;
        }
        
        if(digits.size() == 8) {
            mask |= (1 << 0);
            digits.push_back(0);
        }
        
        return true;
    }
    
    bool isValidSudoku(vector<vector<uint8_t>>& sudoku) {
        bool rows[9][10] = {false};
        bool cols[9][10] = {false};
        bool subs[9][10] = {false};
        
        for(uint8_t i = 0; i < 9; ++i) {
            for(uint8_t j = 0; j < 9; ++j) {
                uint8_t s = i / 3 * 3 + j / 3, num = sudoku[i][j];
                
                if(rows[i][num] || cols[j][num] || subs[s][num]) return false;

                rows[i][num] = cols[j][num] = subs[s][num] = true;
            }
        }
        
        return true;
    }
    
    void printSudoku(vector<vector<uint8_t>>& sudoku) {
        for(vector<uint8_t>& row : sudoku) reverse(row.begin(), row.end());
        
        cout << "-------------------------" << endl;
        for (int i = 0; i < 9; ++i) {
            cout << "| ";
            for (int j = 0; j < 9; ++j) {
                cout << (int)sudoku[i][j] << " ";
                if ((j + 1) % 3 == 0) cout << "| ";
            }
            
            cout << endl;
        
            if ((i + 1) % 3 == 0) std::cout << "-------------------------" << endl;
        }
    }
};

// ************************************************************************************

int main(int argc, const char * argv[]) {
    
    SomewhatSquareSudoku sudoku;
    return sudoku.solve();
}
