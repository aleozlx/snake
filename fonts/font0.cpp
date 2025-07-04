// Font bitmap data for 5x7 character rendering
// Each character is defined as a 7x5 boolean array

extern const bool font_5x7[36][7][5] = {
    // 'A' (index 0)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'B' (index 1)
    {
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'C' (index 2)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'D' (index 3)
    {
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'E' (index 4)
    {
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {1,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0}
    },
    // 'F' (index 5)
    {
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0}
    },
    // 'G' (index 6)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,0},
        {1,0,1,1,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'H' (index 7)
    {
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'I' (index 8)
    {
        {1,1,1,1,1},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0}
    },
    // 'J' (index 9)
    {
        {0,0,0,0,1},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'K' (index 10)
    {
        {1,0,0,0,1},
        {1,0,0,1,0},
        {1,0,1,0,0},
        {1,1,0,0,0},
        {1,0,1,0,0},
        {1,0,0,1,0},
        {1,0,0,0,1}
    },
    // 'L' (index 11)
    {
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0}
    },
    // 'M' (index 12)
    {
        {1,0,0,0,1},
        {1,1,0,1,1},
        {1,0,1,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'N' (index 13)
    {
        {1,0,0,0,1},
        {1,1,0,0,1},
        {1,0,1,0,1},
        {1,0,0,1,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'O' (index 14)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'P' (index 15)
    {
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,0,0,0,0}
    },
    // 'R' (index 16)
    {
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,1,1,1,0},
        {1,0,1,0,0},
        {1,0,0,1,1},
        {0,0,0,0,0}
    },
    // 'S' (index 17)
    {
        {0,1,1,1,1},
        {1,0,0,0,0},
        {1,0,0,0,0},
        {0,1,1,1,0},
        {0,0,0,0,1},
        {1,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'T' (index 18)
    {
        {1,1,1,1,1},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,0,0,0}
    },
    // 'U' (index 19)
    {
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {0,0,0,0,0}
    },
    // 'V' (index 20)
    {
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,0,1,0},
        {0,0,1,0,0},
        {0,0,0,0,0}
    },
    // 'W' (index 21)
    {
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {1,0,1,0,1},
        {1,1,0,1,1},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'X' (index 22)
    {
        {1,0,0,0,1},
        {0,1,0,1,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,1,0,1,0},
        {1,0,0,0,1},
        {0,0,0,0,0}
    },
    // 'Y' (index 23)
    {
        {1,0,0,0,1},
        {0,1,0,1,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,0,0,0}
    },
    // '_' (index 24)
    {
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {1,1,1,1,1},
        {0,0,0,0,0}
    },
    // ' ' (space, index 25)
    {
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0},
        {0,0,0,0,0}
    },
    // '0' (index 26)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,1,1},
        {1,0,1,0,1},
        {1,1,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    // '1' (index 27)
    {
        {0,0,1,0,0},
        {0,1,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,0,1,0,0},
        {0,1,1,1,0}
    },
    // '2' (index 28)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {1,1,1,1,1}
    },
    // '3' (index 29)
    {
        {1,1,1,1,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,0,0,1,0},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    // '4' (index 30)
    {
        {0,0,0,1,0},
        {0,0,1,1,0},
        {0,1,0,1,0},
        {1,0,0,1,0},
        {1,1,1,1,1},
        {0,0,0,1,0},
        {0,0,0,1,0}
    },
    // '5' (index 31)
    {
        {1,1,1,1,1},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {0,0,0,0,1},
        {0,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    // '6' (index 32)
    {
        {0,0,1,1,0},
        {0,1,0,0,0},
        {1,0,0,0,0},
        {1,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    // '7' (index 33)
    {
        {1,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,0,1,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0},
        {0,1,0,0,0}
    },
    // '8' (index 34)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,0}
    },
    // '9' (index 35)
    {
        {0,1,1,1,0},
        {1,0,0,0,1},
        {1,0,0,0,1},
        {0,1,1,1,1},
        {0,0,0,0,1},
        {0,0,0,1,0},
        {0,1,1,0,0}
    }
};

// Function to get character index from character
int getCharIndex(char c) {
    switch(c) {
        case 'A': return 0;
        case 'B': return 1;
        case 'C': return 2;
        case 'D': return 3;
        case 'E': return 4;
        case 'F': return 5;
        case 'G': return 6;
        case 'H': return 7;
        case 'I': return 8;
        case 'J': return 9;
        case 'K': return 10;
        case 'L': return 11;
        case 'M': return 12;
        case 'N': return 13;
        case 'O': return 14;
        case 'P': return 15;
        case 'R': return 16;
        case 'S': return 17;
        case 'T': return 18;
        case 'U': return 19;
        case 'V': return 20;
        case 'W': return 21;
        case 'X': return 22;
        case 'Y': return 23;
        case '_': return 24;
        case ' ': return 25;
        case '0': return 26;
        case '1': return 27;
        case '2': return 28;
        case '3': return 29;
        case '4': return 30;
        case '5': return 31;
        case '6': return 32;
        case '7': return 33;
        case '8': return 34;
        case '9': return 35;
        default: return -1; // Unknown character
    }
}
