#include <stdio.h>
#include "xoshiro.h"
#include <random>
#include <array>

constexpr const size_t N_RANDOM_PLAYS = 10;

// Returns 1..n
inline int Random1toN(int n, Xoshiro256StarStar& rng) {
    return static_cast<int>(RandomRange(n, rng)) + 1;
}

inline void GenerateSet(int out[6], Xoshiro256StarStar& rng) {
    // Insert each incoming number into the sorted prefix [0..i-1] when adding position i
    for (int i = 0; i < 5; ++i) {
        int v = Random1toN(69, rng);
        int pos = i;
        // shift larger elements right until we find the insertion point
        while (pos > 0 && out[pos - 1] > v) {
            out[pos] = out[pos - 1];
            --pos;
        }
        out[pos] = v;
    }
    out[5] = Random1toN(24, rng);
}

void PrintToConsole();

Xoshiro256StarStar rng(12345);
int sameRandom[6];
int tenRandom[N_RANDOM_PLAYS][6];
int lottoNumber[6];
int numRolls = 0;
int numWins = 0;
int numStaticWins = 0;
int staticMatches[6] = { 0,0,0,0,0 };
int matchNums[6] = { 0,0,0,0,0 };

int main()
{
    GenerateSet(sameRandom, rng);

    int printToConsole = 0;
    while (true)
        //for (int l = 0; l < 1000; ++l)
    {
        ++numRolls;

        for (int j = 0; j < N_RANDOM_PLAYS; ++j) {
            GenerateSet(tenRandom[j], rng);
        }

        GenerateSet(lottoNumber, rng);

        bool foundMatch = false;
        // Check random wins
        for (int j = 0; j < N_RANDOM_PLAYS; ++j)
        {
            int numMatches = 0;
            for (int k = 0; k < 5; ++k)
            {
                numMatches += (tenRandom[j][k] == lottoNumber[k]);
            }

            if (numMatches == 5 && tenRandom[j][5] == lottoNumber[5])
            {
                foundMatch = true;
                ++numMatches; // count the 6th (power) match as in original code
                ++numWins;
                break;
            }

            if (numMatches > 0)
            {
                ++matchNums[numMatches - 1];
            }
        }

        // Check static wins
        {
            int numMatches = 0;
            for (int k = 0; k < 5; ++k)
            {
                numMatches += (sameRandom[k] == lottoNumber[k]);
            }
            if (numMatches == 5 && sameRandom[5] == lottoNumber[5])
            {
                foundMatch = true;
                ++numMatches;
                ++numStaticWins;
            }
            if (numMatches > 0)
            {
                ++staticMatches[numMatches - 1];
            }
        }

        if (printToConsole++ >= 1000000)
        {
            printToConsole = 0;
            PrintToConsole();
        }
    }

    return 0;
}

void PrintToConsole()
{
    // Clear console
    system("cls");

    // Num rolls
    printf("-----------------------------\n");
    printf("  Evaluationg . . .\n");
    printf("  Number of Rolls: %d\n", numRolls);
    printf("-----------------------------\n");

    // Stats
    printf("Win stats: \n");
    printf("  Static numbers: %d wins in %d rolls (%.6f%%)\n",
        numStaticWins, numRolls, (static_cast<double>(numStaticWins) / static_cast<double>(numRolls)) * 100.0);
    printf("  Static number match statistics:\n");
    printf("    Match 1: %d\n", staticMatches[0]);
    printf("    Match 2: %d\n", staticMatches[1]);
    printf("    Match 3: %d\n", staticMatches[2]);
    printf("    Match 4: %d\n", staticMatches[3]);
    printf("    Match 5: %d\n", staticMatches[4]);
    printf("  Random Numbers: %d wins in %d rolls (%.6f%%)\n",
        numWins, numRolls, (static_cast<double>(numWins) / static_cast<double>(numRolls)) * 100.0);
    printf("  Random number match statistics:\n");
    printf("    Match 1: %d\n", matchNums[0]);
    printf("    Match 2: %d\n", matchNums[1]);
    printf("    Match 3: %d\n", matchNums[2]);
    printf("    Match 4: %d\n", matchNums[3]);
    printf("    Match 5: %d\n", matchNums[4]);
    printf("-----------------------------\n");

    // Print numbers
    printf("Statistically chosen numbers: %02d %02d %02d %02d %02d %02d\n",
        sameRandom[0], sameRandom[1], sameRandom[2], sameRandom[3], sameRandom[4], sameRandom[5]);

    printf("Generating random plays: \n");
    for (int j = 0; j < N_RANDOM_PLAYS; ++j) {
        printf("  | Play %02d: %02d %02d %02d %02d %02d %02d\n",
            j + 1,
            tenRandom[j][0], tenRandom[j][1], tenRandom[j][2], tenRandom[j][3], tenRandom[j][4], tenRandom[j][5]);
    }

    printf("Lotto Number: %02d %02d %02d %02d %02d %02d\n",
        lottoNumber[0], lottoNumber[1], lottoNumber[2], lottoNumber[3], lottoNumber[4], lottoNumber[5]);
}