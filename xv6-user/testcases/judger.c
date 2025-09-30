#include "test.h"

#define MAX_LINES 100
#define MAX_LENGTH 256
#define TEST_CASES 2
#define MAX_PROCESSES 5


const char* expected[TEST_CASES] = {
    "FIFO Test PASSED",
    "LRU Test PASSED",
};

const char* error = "ERROR";

int simple_strcmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return 1;
        if (s1[i] == '\0') return 1;
    }
    return 0;
}

int find_substring(const char* text, const char* pattern) {
    if (text == NULL || pattern == NULL) {
        return -1;
    }
    
    int pattern_len = 0;
    while (pattern[pattern_len] != '\0') {
        pattern_len++;
    }
    if (pattern_len == 0) {
        return 0;
    }
    
    int i = 0;
    while (text[i] != '\0') {
        if (text[i + pattern_len - 1] == '\0') {
            break;
        }
        if (simple_strcmp(text + i, pattern, pattern_len) == 0) {
            return i;
        }
        i++;
    }
    return -1; 
}


int main(int argc, char* argv[]) {
    printf("Judger: Starting evaluation\n");
    int score = 0;
    
    if (argc == 3) {
        // Test finish order
        char* program_name = argv[1];
        char* output = argv[2];
        int index = -1;
        printf("Test%s output:\n%s\n", program_name, output);
        switch (program_name[0]) {
            case '1': // fifo
                index = 0;
                break;
            case '2': // lru
                index = 1;
                break;
        }
        int res = find_substring(output, expected[index]);
        if (res > 0) {
            if (find_substring(output, error) <= 0) {
                score = 1;
                printf("TEST %s PASSED\n", program_name);
            } else {
                printf("Error: Found ERROR in test case output\n");
            }
        } else {
            printf("Error: Not found expected output\n");
        }
        
    } else {
        printf("Error: Not matched arguments\n");
    }
    
    printf("SCORE: %d\n", score);
    exit(0);
}