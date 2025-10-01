/*
 * Chess Engine V0.5
 *
 * (C) 2025 Tommy Ciccone All Rights Reserved.
*/

#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include "omp.h" // Include OpenMP for parallel processing. Need to install omp to build.

using namespace std;

class Timer {
    public:
        void start() {
            startTime = chrono::high_resolution_clock::now();
        }
        void stop() {
            endTime = chrono::high_resolution_clock::now();
        }
        string getTime() {
            return to_string(chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count());
        }
    private:
        chrono::high_resolution_clock::time_point startTime;
        chrono::high_resolution_clock::time_point endTime;
};

int engineDepth = 5;
int engineBranches = 10;

bool whiteKingMoved = 0, blackKingMoved = 0, whiteLeftRookMoved = 0, 
    whiteRightRookMoved = 0, blackLeftRookMoved = 0, blackRightRookMoved = 0;

bool castled = false;

int positionsEvaluated = 0;
char board[8][8]; // 8x8 chess board

void initializeBoard() { // place default pieces on board
    string blackPieces = "rnbqkbnr";
    string whitePieces = "RNBQKBNR";

    for (int i = 0; i < 8; i++) {
        board[0][i] = blackPieces[i];
        board[1][i] = 'p';
        board[6][i] = 'P';
        board[7][i] = whitePieces[i];
        for (int j = 2; j < 6; j++) {
            board[j][i] = '.';
        }
    }
}

void printBoard() { // print board to console
    cout << "\n";
    for (int i = 0; i < 8; i++) {
        cout << "\033[90m" << 8 - i << " \033[0m";  // rank
        for (int j = 0; j < 8; j++) {
            cout << board[i][j] << ' ';             // piece
        }
        cout << "\n";
    }
    cout << "\033[90m  a b c d e f g h\n\n\033[0m"; // file
}

int immediateEvaluation(bool isOpening = false) {
    int evaluation = 0;
    bool whiteWins = true;
    bool blackWins = true;
    char piece;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            piece = board[i][j];

            switch (piece) { // material evaluation
                case 'P': evaluation += 10; break;
                case 'N': evaluation += 30; break;
                case 'B': evaluation += 30; break;
                case 'R': evaluation += 50; break;
                case 'Q': evaluation += 90; break;
                case 'K': evaluation += 100000; whiteWins = false; break;

                case 'p': evaluation -= 10; break;
                case 'n': evaluation -= 30; break;
                case 'b': evaluation -= 30; break;
                case 'r': evaluation -= 50; break;
                case 'q': evaluation -= 90; break;
                case 'k': evaluation -= 100000; blackWins = false; break;
                default: break;
            }

            if (castled) {
                evaluation -= 4; // bonus/penalty for castling (kept as original)
            }

            // minor piece development
            if (piece == 'N' || piece == 'B') {
                if (i < 6) evaluation += 2;
            }
            if (piece == 'n' || piece == 'b') {
                if (i > 1) evaluation -= 2;
            }

            // centralized knights
            if (piece == 'N' && (i >= 2 && i <= 5 && j >= 2 && j <= 5)) evaluation += 2;
            if (piece == 'n' && (i >= 2 && i <= 5 && j >= 2 && j <= 5)) evaluation -= 2;

            // defended pawns
            if (piece == 'P' && i > 0) {
                if (j > 0 && board[i-1][j-1] == 'P') evaluation += 1;
                if (j < 7 && board[i-1][j+1] == 'P') evaluation += 1;
            }
            if (piece == 'p' && i < 7) {
                if (j > 0 && board[i+1][j-1] == 'p') evaluation -= 1;
                if (j < 7 && board[i+1][j+1] == 'p') evaluation -= 1;
            }

            // advanced pawns
            if (piece == 'P' && i < 5) evaluation += 1;
            if (piece == 'p' && i > 2) evaluation -= 1;

            // center control
            if (piece == 'P') {
                if ((i == 3 || i == 4) && (j == 3 || j == 4)) {
                    evaluation += 5;
                    if (isOpening) evaluation += 3; // extra bonus for center control in opening
                }
            }
            if (piece == 'p') {
                if ((i == 3 || i == 4) && (j == 3 || j == 4)) {
                    evaluation -= 5;
                    if (isOpening) evaluation -= 3; // extra bonus for center control in opening
                }
            }
        }
    }
    positionsEvaluated++;
    return evaluation;
}

bool inBounds(int r, int f) { // check if a target square is on the board
    return r >= 0 && r < 8 && f >= 0 && f < 8;
}

vector<string> enumeratePawnMoves(int r, int f, char piece) { // list all possible pawn moves for a given pawn
    vector<string> moves;
    int direction = 0;
    switch (piece) { // get forwards direction of pawn
        case 'P': direction = -1; break;
        case 'p': direction = 1; break;
    }

    if (inBounds(r + direction, f) && board[r + direction][f] == '.') { // check if square in front is empty
        moves.push_back(to_string(r) + to_string(f) + to_string(r + direction) + to_string(f));
        if ((piece == 'P' && r == 6) || (piece == 'p' && r == 1)) { // check if pawn is on starting square
            if (inBounds(r + 2 * direction, f) && board[r + 2 * direction][f] == '.') { // then check two ahead
                moves.push_back(to_string(r) + to_string(f) + to_string(r + 2*direction) + to_string(f));
            }
        }
    }

    // captures
    if (inBounds(r + direction, f - 1) && islower(board[r + direction][f - 1]) != islower(piece) && board[r + direction][f - 1] != '.') {
        moves.push_back(to_string(r) + to_string(f) + to_string(r + direction) + to_string(f - 1));
    }

    if (inBounds(r + direction, f + 1) && islower(board[r + direction][f + 1]) != islower(piece) && board[r + direction][f + 1] != '.') {
        moves.push_back(to_string(r) + to_string(f) + to_string(r + direction) + to_string(f + 1));
    }
    return moves;
}

vector<string> enumerateKnightMoves(int r, int f, char piece) { // list all possible knight moves for a given knight
    vector<string> moves;
    int knightMoves[8][2] = {{2, -1}, {2, 1}, {-2, -1}, {-2, 1}, {1, -2}, {1, 2}, {-1, -2}, {-1, 2}}; // knight move patterns

    for (int i = 0; i < 8; i++) { // check each knight move pattern, capture or open square
        int tr = r + knightMoves[i][0];
        int tf = f + knightMoves[i][1];
        if (inBounds(tr, tf)) {
            if (board[tr][tf] == '.' || islower(board[tr][tf]) != islower(piece)) {
                moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
            }
        }
    }
    return moves;
}

vector<string> enumerateBishopMoves(int r, int f, char piece) { // list all possible bishop moves for a given bishop
    vector<string> moves;
    int bishopDirections[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // bishop move directions
    for (int i = 0; i < 4; i++) {
        int dr = bishopDirections[i][0];
        int df = bishopDirections[i][1];
        int tr = r + dr;
        int tf = f + df;
        while (inBounds(tr, tf)) { // keep moving in each direction until edge of board or blocked
            if (board[tr][tf] == '.') {
                moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
            } else {
                if (islower(board[tr][tf]) != islower(piece)) { // capture if blocked by other color
                    moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
                }
                break;
            }
            tr += dr;
            tf += df;
        }
    }
    return moves;
}

vector<string> enumerateRookMoves(int r, int f, char piece) { // list all possible rook moves for a given rook
    vector<string> moves;
    int rookDirections[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}}; // rook move directions
    for (int i = 0; i < 4; i++) {
        int dr = rookDirections[i][0];
        int df = rookDirections[i][1];
        int tr = r + dr;
        int tf = f + df;
        while (inBounds(tr, tf)) { // keep moving in each direction until edge of board or blocked
            if (board[tr][tf] == '.') {
                moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
            } else {
                if (islower(board[tr][tf]) != islower(piece)) { // capture if blocked by other color
                    moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
                }
                break;
            }
            tr += dr;
            tf += df;
        }
    }
    return moves;
}

vector<string> enumerateQueenMoves(int r, int f, char piece) { // list all possible queen moves for a given queen
    vector<string> moves;
    int queenDirections[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // queen move directions
    for (int i = 0; i < 8; i++) {
        int dr = queenDirections[i][0];
        int df = queenDirections[i][1];
        int tr = r + dr;
        int tf = f + df;
        while (inBounds(tr, tf)) { // keep moving in each direction until edge of board or blocked
            if (board[tr][tf] == '.') {
                moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
            } else {
                if (islower(board[tr][tf]) != islower(piece)) { // capture if blocked by other color
                    moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
                }
                break;
            }
            tr += dr;
            tf += df;
        }
    }
    return moves;
}

vector<string> enumerateKingMoves(int r, int f, char piece) { // list all possible king moves for a given king
    vector<string> moves;
    int kingDirections[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}}; // king move directions
    for (int i = 0; i < 8; i++) {
        int tr = r + kingDirections[i][0];
        int tf = f + kingDirections[i][1];
        if (inBounds(tr, tf)) {
            if (board[tr][tf] == '.' || islower(board[tr][tf]) != islower(piece)) {
                moves.push_back(to_string(r) + to_string(f) + to_string(tr) + to_string(tf));
            }
        }
    }
/*  if (piece == 'K' && !whiteKingMoved) { // white castling
        if (!whiteLeftRookMoved && board[7][1] == '.' && board[7][2] == '.' && board[7][3] == '.') {
            moves.push_back("7472Q"); // queenside
        }
        if (!whiteRightRookMoved && board[7][5] == '.' && board[7][6] == '.') {
            moves.push_back("7476K"); // kingside
        }
    }
    if (piece == 'k' && !blackKingMoved) { // black castling
        if (!blackLeftRookMoved && board[0][1] == '.' && board[0][2] == '.' && board[0][3] == '.') {
            moves.push_back("0402Q"); // queenside
        }
        if (!blackRightRookMoved && board[0][5] == '.' && board[0][6] == '.') {
            moves.push_back("0406K"); // kingside
        }
    }*/
    return moves;
}

vector<string> enumeratePieceMoves(int r, int f) {
    vector<string> moves;
    char piece = board[r][f];
    if (piece == '.') return moves; // no piece

    switch (tolower(piece)) { // get moves for piece type
        case 'p': moves = enumeratePawnMoves(r, f, piece); break;
        case 'n': moves = enumerateKnightMoves(r, f, piece); break;
        case 'b': moves = enumerateBishopMoves(r, f, piece); break;
        case 'r': moves = enumerateRookMoves(r, f, piece); break;
        case 'q': moves = enumerateQueenMoves(r, f, piece); break;
        case 'k': moves = enumerateKingMoves(r, f, piece); break;
    }
    return moves; // return list of moves
}

vector<string> enumerateAllMoves(bool whiteToMove) {
    vector<string> moves;
    for (int r = 0; r < 8; r++) { // make every move for every piece of color
        for (int f = 0; f < 8; f++) {
            if (board[r][f] != '.' && (isupper(board[r][f]) == whiteToMove)) {
                vector<string> pieceMoves = enumeratePieceMoves(r, f);
                moves.insert(moves.end(), pieceMoves.begin(), pieceMoves.end());
            }
        }
    }
    return moves;
}

int enumerateMoveTree(int depth, int branches, bool whiteToMove, int currentEval) { // recursive evaluation of position
    if (depth == 0) return immediateEvaluation(); // base case

    if (depth < (engineDepth - 2)) { // basic pruning code
        if (currentEval - immediateEvaluation() < -10) {
            return immediateEvaluation();
        } 
    }

    vector<string> moves = enumerateAllMoves(whiteToMove); // get moves
    if (whiteToMove) { // for white
        int te = -10000000; // initial value
        #pragma omp parallel for // multithread with OpenMP
        for (string move : moves) {
            int r = move[0] - '0'; // make move
            int f = move[1] - '0';
            int tr = move[2] - '0';
            int tf = move[3] - '0';
            char captured = board[tr][tf];
            board[tr][tf] = board[r][f];
            board[r][f] = '.';
            int evaluation = enumerateMoveTree(depth - 1, branches, false, currentEval); // evaluate
            board[r][f] = board[tr][tf]; // undo move
            board[tr][tf] = captured;
            te = max(te, evaluation);
        }
        return te; // return evaluation
    } else { // for black
        int te = 10000000; 
        #pragma omp parallel for // multithread with OpenMP
        for (string move : moves) {
            int r = move[0] - '0';
            int f = move[1] - '0';
            int tr = move[2] - '0';
            int tf = move[3] - '0';
            char captured = board[tr][tf];
            board[tr][tf] = board[r][f];
            board[r][f] = '.';
            if (move.length() == 5) { //castling if K or Q is appended to move tag
                if (move[4] == 'K') {
                    board[0][5] = 'r';
                    board[0][7] = '.';
                }
                if (move[4] == 'Q') {
                    board[0][3] = 'r';
                    board[0][0] = '.';
                }
            }
            int evaluation = enumerateMoveTree(depth - 1, branches, true, currentEval);
            board[r][f] = board[tr][tf];
            board[tr][tf] = captured;
            if (move.length() == 5) { // undo castling move
                if (move[4] == 'K') {
                    board[0][7] = 'r';
                    board[0][5] = '.';
                }
                if (move[4] == 'Q') {
                    board[0][0] = 'r';
                    board[0][3] = '.';
                }
            }
            te = min(te, evaluation);
        }
        return te;
    }
}

string selector(int depth, int branches, int currentEval) { // select best move for black
    char backupBoard[8][8]; // Backup board data in case something goes wrong in selection
    memcpy(backupBoard, board, sizeof(board));
    bool backupWhiteKingMoved = whiteKingMoved;
    bool backupBlackKingMoved = blackKingMoved;
    bool backupWhiteLeftRookMoved = whiteLeftRookMoved;
    bool backupWhiteRightRookMoved = whiteRightRookMoved;
    bool backupBlackLeftRookMoved = blackLeftRookMoved;
    bool backupBlackRightRookMoved = blackRightRookMoved;
    bool backupCastled = castled;

    vector<string> moves = enumerateAllMoves(false); // black to move
    string bestMove;
    int te = 10000000;
    #pragma omp parallel for // multithread with OpenMP
    for (string move : moves) {
        int r = move[0] - '0'; // make move
        int f = move[1] - '0';
        int tr = move[2] - '0';
        int tf = move[3] - '0';
        char captured = board[tr][tf];
        board[tr][tf] = board[r][f];
        board[r][f] = '.';
        if (move.length() == 5) { // castling if K or Q is appended to move tag
            if (move[4] == 'K') {
                board[0][5] = 'r';
                board[0][7] = '.';
            }
            if (move[4] == 'Q') {
                board[0][3] = 'r';
                board[0][0] = '.';
            }
        }
        int evaluation = enumerateMoveTree(depth - 1, branches, true, currentEval); // evaluate
        board[r][f] = board[tr][tf]; // undo move
        board[tr][tf] = captured;
        if (move.length() == 5) { // undo castling move
            if (move[4] == 'K') {
                board[0][7] = 'r';
                board[0][5] = '.';
            }
            if (move[4] == 'Q') {
                board[0][0] = 'r';
                board[0][3] = '.';
            }
        }
        if (evaluation < te) { // if better than previous best move, set as new best move
            te = evaluation;
            bestMove = move;
        }
    }
    return bestMove; // return best move

    memcpy(board, backupBoard, sizeof(board)); // restore original board
    whiteKingMoved = backupWhiteKingMoved;
    blackKingMoved = backupBlackKingMoved;
    whiteLeftRookMoved = backupWhiteLeftRookMoved;
    whiteRightRookMoved = backupWhiteRightRookMoved;
    blackLeftRookMoved = backupBlackLeftRookMoved;
    blackRightRookMoved = backupBlackRightRookMoved;
    castled = backupCastled;
}

string convertToCoordinates(string algebraic) { // convert lan to coordinates
    string files = "abcdefgh";
    string ranks = "87654321";

    int fromRow = ranks.find(algebraic[1]);
    int fromCol = files.find(algebraic[0]); 
    int toRow = ranks.find(algebraic[3]);
    int toCol = files.find(algebraic[2]);

    return to_string(fromRow) + to_string(fromCol) + to_string(toRow) + to_string(toCol);
}

string convertToAlgebraic(string coordinates) { // convert coordinates to lan
    string files = "abcdefgh";
    string ranks = "87654321";

    int fromRow = coordinates[0] - '0';
    int fromCol = coordinates[1] - '0';
    int toRow = coordinates[2] - '0';
    int toCol = coordinates[3] - '0';

    return string(1, files[fromCol]) + string(1, ranks[fromRow]) + string(1, files[toCol]) + string(1, ranks[toRow]);
}

void whiteCastleCheck(int r, int f) {
    if (r == 7 && f == 4) whiteKingMoved = true;
    if (r == 7 && f == 0) whiteLeftRookMoved = true;
    if (r == 7 && f == 7) whiteRightRookMoved = true;
}

void blackCastleCheck(int r, int f) {
    if (r == 0 && f == 4) blackKingMoved = true;
    if (r == 0 && f == 0) blackLeftRookMoved = true;
    if (r == 0 && f == 7) blackRightRookMoved = true;
}

int main(int argc, char* argv[]) {
    if (argc == 2){
        engineDepth = stoi(argv[1]);
    }

    string move;
    string response;
    int moveCount = 0;
    bool moveValid;
    Timer timer;

    cout << "Welcome to Chess Engine V0.5\n";
    cout << "(C) 2025 Tommy Ciccone All Rights Reserved.\n";

    initializeBoard();
    printBoard();
    cout << "Evaluation: 0\n\n";

    while (true) {
        cout << "Enter your move in Long Algebraic Notation or type quit to exit\n";
        cout << "> ";
        cin >> move;
        if (move == "quit") break;

        string coordinates = convertToCoordinates(move);
        int r = coordinates[0] - '0';
        int f = coordinates[1] - '0';
        int tr = coordinates[2] - '0';
        int tf = coordinates[3] - '0';

        vector<string> legalMoves = enumerateAllMoves(true);
        for (string lm : legalMoves) {
            if (lm == coordinates) moveValid = true;
        }

        if (!moveValid) {
            cout << "\nIllegal move, try again.\n\n";
            moveValid = false;
            continue;
        }

        moveCount++;

        moveValid = false;

        whiteCastleCheck(r, f);

        board[tr][tf] = board[r][f];
        board[r][f] = '.';

        printBoard();
        cout << "Evaluation: " << immediateEvaluation() << "\n\n";

        cout << "Black is thinking...\n\n";
        positionsEvaluated = 0;
        
        timer.start();
        response = selector(engineDepth, engineBranches, immediateEvaluation());
        timer.stop();
        if (response.size() < 4) {
            cout << "Black has no legal moves. Game over.\n";
            break;
        }
        
        cout << "Black plays: " << convertToAlgebraic(response) << "\n";
        cout << "Evaluated " << positionsEvaluated << " positions in " << timer.getTime() << " seconds.\n";

        int br = response[0] - '0';
        int bf = response[1] - '0';
        int btr = response[2] - '0';
        int btf = response[3] - '0';

        blackCastleCheck(br, bf);

        board[btr][btf] = board[br][bf];
        board[br][bf] = '.';

        if (response.length() == 5 && (response[4] == 'K' || response[4] == 'Q')) {
            if (response[4] == 'K' && board[0][7] == 'r') {
                board[0][5] = 'r';
                board[0][7] = '.';
            }
            if (response[4] == 'Q' && board[0][0] == 'r') {
                board[0][3] = 'r';
                board[0][0] = '.';
            }
            castled = true;
        }

        printBoard();
        cout << "Evaluation: " << immediateEvaluation() << "\n\n";
    }
    return 0;
}