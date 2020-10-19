/*************************************************************
 *
 * This program implements a chess game.
 *
 * Supported variants:
 *
 * Planned variants: classic, hellish acceleration, crazyhouse, chess960, <...>
 *
 * by Maxim Pupykin, 2020
 *
 ************************************************************/

 // ѕќ—Ћ≈ƒЌяя –јЅќ„јя ¬≈–—»я Ќј √»“’јЅ≈

#include <iostream>
#include <cstring>
#include <utility>
#include <string>
#include <ctime>
#include <vector>
#include <algorithm>

const bool WHITE = false;
const bool BLACK = true;
const unsigned short MAX_CHECKING_PIECES = 2;
const unsigned short MAX_MOVES = 30;
char FEN[90]{ "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" };

bool player_to_move = WHITE;
bool castle[4]{ true };

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum class results {
	GAME_IN_PROGRESS = -1,
	DRAW,
	WHITE_WINS,
	BLACK_WINS
};

enum class causes {
	GAME_IN_PROGRESS = -1,
	CHECKMATE,
	RESIGNATION,
	AGREEMENT_TO_A_DRAW,
	UNSUFFICIENT_MATERIAL,
	WHITE_OUT_OF_TIME,
	BLACK_OUT_OF_TIME,
	THREEFOLD_REPETITION,
	FIVEFOLD_REPETITION,
	BY_50_MOVE_RULE,
	BY_75_MOVE_RULE,
};

struct game_results {
	results result;
	causes cause;

	game_results() : result(results::GAME_IN_PROGRESS), cause(causes::GAME_IN_PROGRESS) {};
};

game_results game_result;

enum piece_types {
	EMPTY,
	KING,
	QUEEN,
	ROOK,
	BISHOP,
	KNIGHT,
	PAWN,
};

enum class game_types {
	CLASSIC,
	CHESS_960,
	HELLISH_ACCELERATION,
	CRAZYHOUSE,
	CHESS_EX,
	KING_OF_THE_HILL,
};

game_types game_type;

enum move_types {
	NO_CAPTURE = 1,
	CAPTURE,
	PROMOTION,
	CAPTURE_WITH_PROMOTION,
	EN_PASSANT,
	LONG_PAWN_MOVE,
	CASTLES,
	PLACEMENT, //дл€ crazyhouse, не реализовано в make_move
};

struct captured_pieces {
	int queen, rook, bishop, knight, rawn;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Position {
public:

	short file;
	short rank;

	Position() : file(0), rank(0) {};
	Position(short new_file, short new_rank) : file(new_file), rank(new_rank) {};

	friend bool operator== (const Position& lhs, const Position& rhs);
};

bool operator== (const Position& lhs, const Position& rhs) {
	return (lhs.file == rhs.file) && (lhs.rank == rhs.rank);
}

bool en_passant = false;

Position en_passant_position(-1, -1);

unsigned short en_passant_cnt = 0;

class Moves {
private:

	struct move {
		Position destination;
		move_types move_type;

		move() : destination(-1, -1), move_type(NO_CAPTURE) {};
		move(short file, short rank, move_types type) : destination(file, rank), move_type(type) {};
	};

	move move_list[MAX_MOVES];

	unsigned short move_cnt;

public:

	Moves() {
		move_cnt = 0;
	}

	~Moves() {
		move_cnt = 0;
		for (auto i = 0; i < MAX_MOVES; ++i) {
			move_list[i].move_type = NO_CAPTURE;
			move_list[i].destination.file = -1;
			move_list[i].destination.rank = -1;
		}
	}

	void add(short file, short rank, move_types type) {
		move_list[move_cnt] = move(file, rank, type);
		move_cnt++;
	}

	void clear() {
		move_cnt = 0;
	}

	unsigned short amount() {
		return move_cnt;
	}

	move& operator[] (const int index) {
		if (index < 0 || index > move_cnt) throw "Invalid index";
		return move_list[index];
	}
};

class piece {
protected:

	Position position;

	bool color;

	bool promoted;

public:

	Moves moves;

	virtual void find(bool mark) = 0;

	virtual bool get_color() {
		return color;
	}

	virtual bool is_promoted() {
		return promoted;
	}

	virtual void set_position(short file, short rank) {
		position.file = file;
		position.rank = rank;
	}

	virtual piece_types type() = 0;
};

class square {
private:

	bool is_occupied;

	int attacked;

public:

	piece* _piece;

	bool color() {
		return _piece->get_color();
	}

	bool occupied() {
		bool res;
		_piece->type() == EMPTY ? res = false : res = true;
		return res;
	}

	void attack() {
		++attacked;
	}

	void unattack() {
		attacked = 0;
	}

	bool is_attacked() {
		return static_cast<bool>(attacked);
	}

	piece_types piece_type() {
		return _piece->type();
	}
};

square board[8][8];

class Checking_pieces {
private:

	struct ch_p {
		Position position;
		piece_types piece_type;

		ch_p() : position(-1, -1), piece_type(EMPTY) {};
		ch_p(short file, short rank, piece_types type) : position(file, rank), piece_type(type) {};
	};

	ch_p piece_list[MAX_CHECKING_PIECES];

	unsigned short piece_cnt;

public:

	Checking_pieces() {
		piece_cnt = 0;
	}

	~Checking_pieces() {
		piece_cnt = 0;
		for (auto i = 0; i < MAX_CHECKING_PIECES; ++i) {
			piece_list[i].piece_type = EMPTY;
			piece_list[i].position.file = -1;
			piece_list[i].position.rank = -1;
		}
	}

	void add(short file, short rank, piece_types type) {
		piece_list[piece_cnt] = ch_p(file, rank, type);
		piece_cnt++;
	}

	void clear() {
		piece_cnt = 0;
	}

	unsigned short amount() {
		return piece_cnt;
	}

	ch_p& operator[] (const int index) {
		if (index < 0 || index > piece_cnt) throw "Invalid index";
		return piece_list[index];
	}
};

Checking_pieces checking_pieces;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class pawn :public piece {
public:

	pawn(bool color, short new_file, short new_rank) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = false;
	};

	virtual void find(bool mark) {
		if (!get_color()) {//дл€ белых
			auto i = position.file;
			auto j = position.rank + 1;
			if (mark) {
				i = position.file + 1;
				if (i < 8) {
					board[i][j].attack();
					if (board[i][j].piece_type() == KING && board[i][j].color() != get_color()) checking_pieces.add(i, j, PAWN);
				}
				i = position.file - 1;
				if (i >= 0) {
					board[i][j].attack();
					if (board[i][j].piece_type() == KING && board[i][j].color() != get_color()) checking_pieces.add(i, j, PAWN);
				}
			}
			else {
				if (!board[i][j].occupied()) {
					j == 7 ? moves.add(i, j, PROMOTION) : moves.add(i, j, NO_CAPTURE);
					if ((position.rank == 1) && !board[i][j + 1].occupied()) moves.add(i, j + 1, LONG_PAWN_MOVE);
				}
				i = position.file + 1;
				if (i < 8) {
					if (board[i][j].occupied() && (board[i][j].color() != player_to_move)) {
						j == 7 ? moves.add(i, j, CAPTURE_WITH_PROMOTION) : moves.add(i, j, CAPTURE);
					}
				}
				i = position.file - 1;
				if (i >= 0) {
					if (board[i][j].occupied() && (board[i][j].color() != player_to_move)) {
						j == 7 ? moves.add(i, j, CAPTURE_WITH_PROMOTION) : moves.add(i, j, CAPTURE);
					}
				}
				if (en_passant && (position.rank == 4)) {//пока не работает
					if ((en_passant_position.file == position.file + 1) || (en_passant_position.file == position.file - 1)) {
						moves.add(en_passant_position.file, en_passant_position.rank, EN_PASSANT);
					}
				}
			}
		}
		if (get_color()) {//дл€ черных
			auto i = position.file;
			auto j = position.rank - 1;
			if (mark) {
				i = position.file + 1;
				if (i < 8) {
					board[i][j].attack();
					if (board[i][j].piece_type() == KING && board[i][j].color() != get_color()) checking_pieces.add(i, j, PAWN);
				}
				i = position.file - 1;
				if (i >= 0) {
					board[i][j].attack();
					if (board[i][j].piece_type() == KING && board[i][j].color() != get_color()) checking_pieces.add(i, j, PAWN);
				}
			}
			else {
				if (!board[i][j].occupied()) {
					j == 7 ? moves.add(i, j, PROMOTION) : moves.add(i, j, NO_CAPTURE);
					if ((position.rank == 6) && !board[i][j - 1].occupied()) moves.add(i, j - 1, LONG_PAWN_MOVE);
				}
				i = position.file + 1;
				if (i < 8) {
					if (board[i][j].occupied() && (board[i][j].color() != player_to_move)) {
						j == 7 ? moves.add(i, j, CAPTURE_WITH_PROMOTION) : moves.add(i, j, CAPTURE);
					}
				}
				i = position.file - 1;
				if (i >= 0) {
					if (board[i][j].occupied() && (board[i][j].color() != player_to_move)) {
						j == 7 ? moves.add(i, j, CAPTURE_WITH_PROMOTION) : moves.add(i, j, CAPTURE);
					}
				}
				if (en_passant && (position.rank == 3)) {
					if ((en_passant_position.file == position.file + 1) || (en_passant_position.file == position.file - 1)) {
						moves.add(en_passant_position.file, en_passant_position.rank, EN_PASSANT);
					}
				}
			}
		}
	}

	virtual piece_types type() {
		return PAWN;
	}
};
//сведени€ об атакованных клетках сохран€ть до конца хода соперника, королю сделать проверку атакованности места назначени€,
			//после хода фигурой атаковать все клетки всми фигурами и сохран€ть до хода соперника
class king :public piece {
public:

	king(bool color, short new_file, short new_rank) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = false;
	};

	virtual void find(bool mark) {
		if (mark) {
			auto i = position.file + 1;
			auto j = position.rank;
			if (i < 8) {
				board[i][j].attack();
			}
			i = position.file - 1;
			if (i >= 0) {
				board[i][j].attack();
			}
			i = position.file;
			j = position.rank - 1;
			if (j >= 0) {
				board[i][j].attack();
			}
			j = position.rank + 1;
			if (j < 8) {
				board[i][j].attack();
			}
			i = position.file + 1;
			if (i < 8 && j < 8) {
				board[i][j].attack();
			}
			i = position.file + 1;
			j = position.rank - 1;
			if (i < 8 && j >= 0) {
				board[i][j].attack();
			}
			i = position.file - 1;
			j = position.rank + 1;
			if (j < 8 && i >= 0) {
				board[i][j].attack();
			}
			i = position.file - 1;
			j = position.rank - 1;
			if (i >= 0 && j >= 0) {
				board[i][j].attack();
			}
		}
		else {
			auto i = position.file + 1;
			auto j = position.rank;
			if (!board[i][j].is_attacked() && i < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file - 1;
			if (!board[i][j].is_attacked() && i >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file;
			j = position.rank - 1;
			if (!board[i][j].is_attacked() && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			j = position.rank + 1;
			if (!board[i][j].is_attacked() && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file + 1;
			if (!board[i][j].is_attacked() && i < 8 && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file + 1;
			j = position.rank - 1;
			if (!board[i][j].is_attacked() && i < 8 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file - 1;
			j = position.rank + 1;
			if (!board[i][j].is_attacked() && j < 8 && i >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
			i = position.file - 1;
			j = position.rank - 1;
			if (!board[i][j].is_attacked() && i >= 0 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) moves.add(i, j, CAPTURE);
				}
			}
		}
	}

	virtual piece_types type() {
		return KING;
	}
};

class knight :public piece {
public:

	knight(bool color, short new_file, short new_rank, bool was_promoted = false) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = was_promoted;
	};

	virtual void find(bool mark) {
		if (mark) {
			auto i = position.file - 1;
			auto j = position.rank - 2;
			if (i >= 0 && j >= 0) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file + 1;
			j = position.rank - 2;
			if (i < 8 && j >= 0) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file + 1;
			j = position.rank + 2;
			if (i < 8 && j < 8) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file - 1;
			j = position.rank + 2;
			if (i >= 0 && j < 8) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file - 2;
			j = position.rank - 1;
			if (i >= 0 && j >= 0) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file + 2;
			j = position.rank - 1;
			if (i < 8 && j >= 0) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file + 2;
			j = position.rank + 1;
			if (i < 8 && j < 8) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}

			i = position.file - 2;
			j = position.rank + 1;
			if (i >= 0 && j < 8) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
				}
				else {
					if (board[i][j].color() != get_color()) {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) checking_pieces.add(i, j, KNIGHT);
					}
					else board[i][j].attack();
				}
			}
		}
		else {
			auto i = position.file - 1;
			auto j = position.rank - 2;
			if (i >= 0 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file + 1;
			j = position.rank - 2;
			if (i < 8 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file + 1;
			j = position.rank + 2;
			if (i < 8 && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file - 1;
			j = position.rank + 2;
			if (i >= 0 && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file - 2;
			j = position.rank - 1;
			if (i >= 0 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file + 2;
			j = position.rank - 1;
			if (i < 8 && j >= 0) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file + 2;
			j = position.rank + 1;
			if (i < 8 && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}

			i = position.file - 2;
			j = position.rank + 1;
			if (i >= 0 && j < 8) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
				}
				else {
					if (board[i][j].color() != get_color()) {
						moves.add(i, j, CAPTURE);
					}
				}
			}
		}
	}

	virtual piece_types type() {
		return KNIGHT;
	}
};

class bishop :public piece {
public:

	bishop(bool color, short new_file, short new_rank, bool was_promoted = false) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = was_promoted;
	};

	virtual void find(bool mark) {
		if (mark) {
			auto j = position.rank + 1;
			auto i = position.file + 1;
			for (; i < 8 && j < 8; ++i, ++j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, BISHOP);
							if (i != 7 && j != 7) board[i + 1][j + 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file + 1;
			for (; i < 8 && j >= 0; ++i, --j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, BISHOP);
							if (i != 7 && j != 0) board[i + 1][j - 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank + 1;
			i = position.file - 1;
			for (; i >= 0 && j < 8; --i, ++j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, BISHOP);
							if (i != 0 && j != 7) board[i - 1][j + 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file - 1;
			for (; i >= 0 && j >= 0; --i, --j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, BISHOP);
							if (i != 0 && j != 0) board[i - 1][j - 1].attack();
						}
						break;
					}
				}
			}
		}
		else {
			auto j = position.rank + 1;
			auto i = position.file + 1;
			for (; i < 8 && j < 8; ++i, ++j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file + 1;
			for (; i < 8 && j >= 0; ++i, --j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank + 1;
			i = position.file - 1;
			for (; i >= 0 && j < 8; --i, ++j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file - 1;
			for (; i >= 0 && j >= 0; --i, --j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}
		}
	}

	virtual piece_types type() {
		return BISHOP;
	}
};

class rook :public piece {
public:

	rook(bool color, short new_file, short new_rank, bool was_promoted = false) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = was_promoted;
	};

	virtual void find(bool mark) {
		if (mark) {
			for (auto i = position.rank + 1; i < 8; ++i) {
				if (!board[position.file][i].occupied()) {
					board[position.file][i].attack();
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) {
						board[position.file][i].attack();
						break;
					}
					else {
						board[position.file][i].attack();
						if (board[position.file][i].piece_type() == KING) {
							checking_pieces.add(position.file, i, ROOK);
							if (i != 7) board[position.file][i + 1].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.rank - 1; i >= 0; --i) {
				if (!board[position.file][i].occupied()) {
					board[position.file][i].attack();
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) {
						board[position.file][i].attack();
						break;
					}
					else {
						board[position.file][i].attack();
						if (board[position.file][i].piece_type() == KING) {
							checking_pieces.add(position.file, i, ROOK);
							if (i != 0) board[position.file][i - 1].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.file + 1; i < 8; ++i) {
				if (!board[i][position.rank].occupied()) {
					board[i][position.rank].attack();
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) {
						board[i][position.rank].attack();
						break;
					}
					else {
						board[i][position.rank].attack();
						if (board[i][position.rank].piece_type() == KING) {
							checking_pieces.add(i, position.rank, ROOK);
							if (i != 7) board[i + 1][position.rank].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.file - 1; i >= 0; --i) {
				if (!board[i][position.rank].occupied()) {
					board[i][position.rank].attack();
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) {
						board[i][position.rank].attack();
						break;
					}
					else {
						board[i][position.rank].attack();
						if (board[i][position.rank].piece_type() == KING) {
							checking_pieces.add(i, position.rank, ROOK);
							if (i != 0) board[i - 1][position.rank].attack();
						}
						break;
					}
				}
			}
		}
		else {
			for (auto i = position.rank + 1; i < 8; ++i) {
				if (!board[position.file][i].occupied()) {
					moves.add(position.file, i, NO_CAPTURE);
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) break;
					else {
						moves.add(position.file, i, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.rank - 1; i >= 0; --i) {
				if (!board[position.file][i].occupied()) {
					moves.add(position.file, i, NO_CAPTURE);
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) break;
					else {
						moves.add(position.file, i, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.file + 1; i < 8; ++i) {
				if (!board[i][position.rank].occupied()) {
					moves.add(i, position.rank, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) break;
					else {
						moves.add(i, position.rank, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.file - 1; i >= 0; --i) {
				if (!board[i][position.rank].occupied()) {
					moves.add(i, position.rank, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) break;
					else {
						moves.add(i, position.rank, CAPTURE);
						break;
					}
				}
			}
		}
	}

	virtual piece_types type() {
		return ROOK;
	}
};

class queen :public piece {
public:

	queen(bool color, short new_file, short new_rank, bool was_promoted = false) {
		position.file = new_file;
		position.rank = new_rank;
		this->color = color;
		this->promoted = was_promoted;
	};

	virtual void find(bool mark) {
		if (mark) {
			for (auto i = position.rank + 1; i < 8; ++i) {
				if (!board[position.file][i].occupied()) {
					board[position.file][i].attack();
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) {
						board[position.file][i].attack();
						break;
					}
					else {
						board[position.file][i].attack();
						if (board[position.file][i].piece_type() == KING) {
							checking_pieces.add(position.file, i, QUEEN);
							if (i != 7) board[position.file][i + 1].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.rank - 1; i >= 0; --i) {
				if (!board[position.file][i].occupied()) {
					board[position.file][i].attack();
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) {
						board[position.file][i].attack();
						break;
					}
					else {
						board[position.file][i].attack();
						if (board[position.file][i].piece_type() == KING) {
							checking_pieces.add(position.file, i, QUEEN);
							if (i != 0) board[position.file][i - 1].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.file + 1; i < 8; ++i) {
				if (!board[i][position.rank].occupied()) {
					board[i][position.rank].attack();
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) {
						board[i][position.rank].attack();
						break;
					}
					else {
						board[i][position.rank].attack();
						if (board[i][position.rank].piece_type() == KING) {
							checking_pieces.add(i, position.rank, QUEEN);
							if (i != 7) board[i + 1][position.rank].attack();
						}
						break;
					}
				}
			}

			for (auto i = position.file - 1; i >= 0; --i) {
				if (!board[i][position.rank].occupied()) {
					board[i][position.rank].attack();
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) {
						board[i][position.rank].attack();
						break;
					}
					else {
						board[i][position.rank].attack();
						if (board[i][position.rank].piece_type() == KING) {
							checking_pieces.add(i, position.rank, QUEEN);
							if (i != 0) board[i - 1][position.rank].attack();
						}
						break;
					}
				}
			}

			auto j = position.rank + 1;
			auto i = position.file + 1;
			for (; i < 8 && j < 8; ++i, ++j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, QUEEN);
							if (i != 7 && j != 7) board[i + 1][j + 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file + 1;
			for (; i < 8 && j >= 0; ++i, --j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, QUEEN);
							if (i != 7 && j != 0) board[i + 1][j - 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank + 1;
			i = position.file - 1;
			for (; i >= 0 && j < 8; --i, ++j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, QUEEN);
							if (i != 0 && j != 7) board[i - 1][j + 1].attack();
						}
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file - 1;
			for (; i >= 0 && j >= 0; --i, --j) {
				if (!board[i][j].occupied()) {
					board[i][j].attack();
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) {
						board[i][j].attack();
						break;
					}
					else {
						board[i][j].attack();
						if (board[i][j].piece_type() == KING) {
							checking_pieces.add(i, j, QUEEN);
							if (i != 0 && j != 0) board[i - 1][j - 1].attack();
						}
						break;
					}
				}
			}
		}
		else {
			for (auto i = position.rank + 1; i < 8; ++i) {
				if (!board[position.file][i].occupied()) {
					moves.add(position.file, i, NO_CAPTURE);
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) break;
					else {
						moves.add(position.file, i, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.rank - 1; i >= 0; --i) {
				if (!board[position.file][i].occupied()) {
					moves.add(position.file, i, NO_CAPTURE);
					continue;
				}
				else {
					if (board[position.file][i].color() == get_color()) break;
					else {
						moves.add(position.file, i, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.file + 1; i < 8; ++i) {
				if (!board[i][position.rank].occupied()) {
					moves.add(i, position.rank, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) break;
					else {
						moves.add(i, position.rank, CAPTURE);
						break;
					}
				}
			}

			for (auto i = position.file - 1; i >= 0; --i) {
				if (!board[i][position.rank].occupied()) {
					moves.add(i, position.rank, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][position.rank].color() == get_color()) break;
					else {
						moves.add(i, position.rank, CAPTURE);
						break;
					}
				}
			}

			auto j = position.rank + 1;
			auto i = position.file + 1;
			for (; i < 8 && j < 8; ++i, ++j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file + 1;
			for (; i < 8 && j >= 0; ++i, --j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank + 1;
			i = position.file - 1;
			for (; i >= 0 && j < 8; --i, ++j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}

			j = position.rank - 1;
			i = position.file - 1;
			for (; i >= 0 && j >= 0; --i, --j) {
				if (!board[i][j].occupied()) {
					moves.add(i, j, NO_CAPTURE);
					continue;
				}
				else {
					if (board[i][j].color() == get_color()) break;
					else {
						moves.add(i, j, CAPTURE);
						break;
					}
				}
			}
		}
	}

	virtual piece_types type() {
		return QUEEN;
	}
};

class no_piece :public piece {
public:

	no_piece(short new_file, short new_rank) {
		position.file = new_file;
		position.rank = new_rank;
	};

	virtual piece_types type() {
		return EMPTY;
	}

	virtual void find(bool mark) {
		throw "This square is empty";
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void initialize_classic() {
	for (auto i = 2; i < 6; ++i) {
		for (auto j = 0; j < 8; ++j) {
			board[j][i]._piece = new no_piece(j, i);
		}
	}
	for (auto i = 1; i < 8; i += 5) {
		for (auto j = 0; j < 8; ++j) {
			if (i == 1) board[j][i]._piece = new pawn(WHITE, j, i);
			if (i == 6) board[j][i]._piece = new pawn(BLACK, j, i);
		}
	}
	board[0][0]._piece = new rook(WHITE, 0, 0);
	board[1][0]._piece = new knight(WHITE, 1, 0);
	board[2][0]._piece = new bishop(WHITE, 2, 0);
	board[3][0]._piece = new queen(WHITE, 3, 0);
	board[4][0]._piece = new king(WHITE, 4, 0);
	board[5][0]._piece = new bishop(WHITE, 5, 0);
	board[6][0]._piece = new knight(WHITE, 6, 0);
	board[7][0]._piece = new rook(WHITE, 7, 0);
	board[0][7]._piece = new rook(BLACK, 0, 7);
	board[1][7]._piece = new knight(BLACK, 1, 7);
	board[2][7]._piece = new bishop(BLACK, 2, 7);
	board[3][7]._piece = new queen(BLACK, 3, 7);
	board[4][7]._piece = new king(BLACK, 4, 7);
	board[5][7]._piece = new bishop(BLACK, 5, 7);
	board[6][7]._piece = new knight(BLACK, 6, 7);
	board[7][7]._piece = new rook(BLACK, 7, 7);
}

void get_960_position(std::string& starting_position) {
	std::vector<int> available_positions;
	for (auto i = 0; i < 8; ++i) {
		available_positions.push_back(i);
	}
	int king_pos, rook_pos_1, rook_pos_2, bishop_pos_1, bishop_pos_2, queen_pos, knight_pos_1, knight_pos_2;
	do {
		king_pos = rand() % 8;
		do {
			rook_pos_1 = rand() % 8;
		} while (rook_pos_1 == king_pos);
		do {
			rook_pos_2 = rand() % 8;
		} while ((rook_pos_2 == king_pos) || (rook_pos_2 == rook_pos_1));
	} while (!(((rook_pos_1 < king_pos) && (king_pos < rook_pos_2)) || ((rook_pos_2 < king_pos) && (king_pos < rook_pos_1))));
	starting_position[king_pos] = 'K';
	starting_position[rook_pos_1] = 'R';
	starting_position[rook_pos_2] = 'R';
	available_positions.erase(std::find(available_positions.begin(), available_positions.end(), king_pos));
	available_positions.erase(std::find(available_positions.begin(), available_positions.end(), rook_pos_1));
	available_positions.erase(std::find(available_positions.begin(), available_positions.end(), rook_pos_2));
	bishop_pos_1 = rand() % 5;
	do {
		bishop_pos_2 = rand() % 5;
	} while ((available_positions[bishop_pos_1] - available_positions[bishop_pos_2]) % 2 == 0);
	starting_position[available_positions[bishop_pos_1]] = 'B';
	starting_position[available_positions[bishop_pos_2]] = 'B';
	std::vector<int>::iterator iter = available_positions.begin();
	for (auto i = 0; i < bishop_pos_1; ++i) ++iter;
	available_positions.erase(iter);
	iter = available_positions.begin();
	if (bishop_pos_2 > bishop_pos_1) {
		for (auto i = 0; i < bishop_pos_2 - 1; ++i) ++iter;
	}
	else {
		for (auto i = 0; i < bishop_pos_2; ++i) ++iter;
	}
	available_positions.erase(iter);
	queen_pos = rand() % 3;
	starting_position[available_positions[queen_pos]] = 'Q';
	iter = available_positions.begin();
	for (auto i = 0; i < queen_pos; ++i) ++iter;
	available_positions.erase(iter);
	knight_pos_1 = 1;
	knight_pos_2 = 0;
	starting_position[available_positions[knight_pos_1]] = 'N';
	starting_position[available_positions[knight_pos_2]] = 'N';
	iter = available_positions.begin();
	for (auto i = 0; i < knight_pos_1; ++i) ++iter;
	available_positions.erase(iter);
	iter = available_positions.begin();
	for (auto i = 0; i < knight_pos_2; ++i) ++iter;
	available_positions.erase(iter);
}

void initialize_960() {
	for (auto i = 2; i < 6; ++i) {
		for (auto j = 0; j < 8; ++j) {
			board[j][i]._piece = new no_piece(j, i);
		}
	}
	for (auto i = 1; i < 8; i += 5) {
		for (auto j = 0; j < 8; ++j) {
			if (i == 1) board[j][i]._piece = new pawn(WHITE, j, i);
			if (i == 6) board[j][i]._piece = new pawn(BLACK, j, i);
		}
	}
	std::string starting_position{ "RNBQKBNR" };
	get_960_position(starting_position);
	for (auto i = 0; i < 8; ++i) {
		switch (starting_position[i]) {
		case 'K':
			board[i][0]._piece = new king(WHITE, i, 0);
			break;
		case 'Q':
			board[i][0]._piece = new queen(WHITE, i, 0);
			break;
		case 'R':
			board[i][0]._piece = new rook(WHITE, i, 0);
			break;
		case 'B':
			board[i][0]._piece = new bishop(WHITE, i, 0);
			break;
		case 'N':
			board[i][0]._piece = new knight(WHITE, i, 0);
			break;
		default:
			throw "Board initialization error";
			break;
		}
	}
	for (auto i = 0; i < 8; ++i) {
		switch (starting_position[i]) {
		case 'K':
			board[i][7]._piece = new king(BLACK, i, 7);
			break;
		case 'Q':
			board[i][7]._piece = new queen(BLACK, i, 7);
			break;
		case 'R':
			board[i][7]._piece = new rook(BLACK, i, 7);
			break;
		case 'B':
			board[i][7]._piece = new bishop(BLACK, i, 7);
			break;
		case 'N':
			board[i][7]._piece = new knight(BLACK, i, 7);
			break;
		default:
			throw "Board initialization error";
			break;
		}
	}
}

void initialize_board() {
	switch (game_type) {
	case game_types::CLASSIC:
		initialize_classic();
		break;
	case game_types::CHESS_960:
		initialize_960();
		break;
	case game_types::HELLISH_ACCELERATION:
		initialize_classic();
		break;
	case game_types::CRAZYHOUSE:
		initialize_classic();
		break;
	case game_types::CHESS_EX:
		initialize_classic();
		break;
	case game_types::KING_OF_THE_HILL:
		initialize_classic();
		break;
	default:
		throw "Board initialization error";
		break;
	}
}

void delete_board() {
	for (auto i = 0; i < 8; ++i) {
		for (auto j = 0; j < 8; ++j) {
			delete board[j][i]._piece;
		}
	}
}

void print_board() {
	for (auto i = 7; i >= 0; --i) {
		for (auto j = 0; j < 8; ++j) {
			piece_types piece_type = board[j][i].piece_type();
			switch (piece_type) {
			case EMPTY:
				std::cout << '_';
				break;
			case KING:
				if (!board[j][i].color()) std::cout << 'K';
				else std::cout << 'k';
				break;
			case QUEEN:
				if (!board[j][i].color()) std::cout << 'Q';
				else std::cout << 'q';
				break;
			case ROOK:
				if (!board[j][i].color()) std::cout << 'R';
				else std::cout << 'r';
				break;
			case BISHOP:
				if (!board[j][i].color()) std::cout << 'B';
				else std::cout << 'b';
				break;
			case KNIGHT:
				if (!board[j][i].color()) std::cout << 'N';
				else std::cout << 'n';
				break;
			case PAWN:
				if (!board[j][i].color()) std::cout << 'P';
				else std::cout << 'p';
				break;
			}
			std::cout << ' ';
		}
		std::cout << std::endl;
	}
}

void mark_attacked() {
	for (auto i = 0; i < 8; ++i) {
		for (auto j = 0; j < 8; ++j) {
			if (board[i][j].occupied() && (board[i][j].color() != player_to_move)) board[i][j]._piece->find(true);
		}
	}
}

void unmark_attacked() {
	for (auto i = 0; i < 8; ++i) {
		for (auto j = 0; j < 8; ++j) {
			board[i][j].unattack();
		}
	}
}

Position find_king() {
	for (auto i = 0; i < 8; ++i) {
		for (auto j = 0; j < 8; ++j) {
			if (board[i][j].occupied() && (board[i][j].color() == player_to_move) && board[i][j].piece_type() == KING) {
				return Position(i, j);
			}
		}
	}
	std::cout << "Error: there is no king on board" << std::endl;
	return Position(-1, -1);
}

bool check_king() {//returns false if king is under check
	bool res;
	checking_pieces.clear();
	Position king_position = find_king();
	mark_attacked();
	board[king_position.file][king_position.rank].is_attacked() ? res = false : res = true;
	unmark_attacked();
	return res;
}

bool no_capture_move(Position source, Position destination) {
	board[destination.file][destination.rank]._piece = board[source.file][source.rank]._piece;
	board[source.file][source.rank]._piece = new no_piece(source.file, source.rank);
	if (!check_king()) {
		std::cout << "Error: your king is in check after the move" << std::endl;
		board[source.file][source.rank]._piece = board[destination.file][destination.rank]._piece;
		board[destination.file][destination.rank]._piece = new no_piece(source.file, source.rank);
		return false;
	}
	board[destination.file][destination.rank]._piece->set_position(destination.file, destination.rank);
	return true;
}

bool capture_move(Position source, Position destination, bool checking = false) {
	auto temp = board[destination.file][destination.rank]._piece;
	board[destination.file][destination.rank]._piece = board[source.file][source.rank]._piece;
	board[source.file][source.rank]._piece = new no_piece(source.file, source.rank);
	if (!check_king()) {
		std::cout << "Error: your king is in check after the move" << std::endl;
		board[source.file][source.rank]._piece = board[destination.file][destination.rank]._piece;
		board[destination.file][destination.rank]._piece = temp; //будут ли проблемы с указател€ми?
		return false;
	}
	if (checking) {
		board[source.file][source.rank]._piece = board[destination.file][destination.rank]._piece;
		board[destination.file][destination.rank]._piece = temp; //будут ли проблемы с указател€ми?
		return true;
	}
	board[destination.file][destination.rank]._piece->set_position(destination.file, destination.rank);
	return true;
}

bool en_passant_move(Position source, Position destination, bool checking = false) {
	piece *temp;
	if (!player_to_move) {
		temp = board[destination.file][destination.rank - 1]._piece;
		board[destination.file][destination.rank - 1]._piece = new no_piece(destination.file, destination.rank - 1);
	}
	else {
		temp = board[destination.file][destination.rank + 1]._piece;
		board[destination.file][destination.rank + 1]._piece = new no_piece(destination.file, destination.rank + 1);
	}
	board[destination.file][destination.rank]._piece = board[source.file][source.rank]._piece;
	board[source.file][source.rank]._piece = new no_piece(source.file, source.rank);
	if (!check_king()) {
		if (!player_to_move) {
			board[destination.file][destination.rank - 1]._piece = temp;
		}
		else {
			board[destination.file][destination.rank + 1]._piece = temp;
		}
		std::cout << "Error: your king is in check after the move" << std::endl;
		board[source.file][source.rank]._piece = board[destination.file][destination.rank]._piece;
		board[destination.file][destination.rank]._piece = new no_piece(source.file, source.rank);
		return false;
	}
	if (checking) {
		if (!player_to_move) {
			board[destination.file][destination.rank - 1]._piece = temp;
		}
		else {
			board[destination.file][destination.rank + 1]._piece = temp;
		}
		board[source.file][source.rank]._piece = board[destination.file][destination.rank]._piece;
		board[destination.file][destination.rank]._piece = new no_piece(source.file, source.rank);
		return true;
	}
	board[destination.file][destination.rank]._piece->set_position(destination.file, destination.rank);
	return true;
}

bool promotion(Position source, Position destination, bool capture = false) { //проверить и доработать
	bool res;
	capture ? res = capture_move(source, destination) : res = no_capture_move(source, destination);
	bool b = true;
	while (b) {
		char c;
		std::cout << "Choose the piece you want to promote your pawn into:" << std::endl;
		std::cout << "q - Queen" << std::endl;
		std::cout << "r - Rook" << std::endl;
		std::cout << "b - Bishop" << std::endl;
		std::cout << "n - Knight" << std::endl;
		std::cin >> c;
		std::cout << std::endl;
		b = false;
		switch (c) {
		case 'q':
			board[destination.file][destination.rank]._piece = new queen(player_to_move, destination.file, destination.rank, true);
			break;
		case 'r':
			board[destination.file][destination.rank]._piece = new rook(player_to_move, destination.file, destination.rank, true);
			break;
		case 'b':
			board[destination.file][destination.rank]._piece = new bishop(player_to_move, destination.file, destination.rank, true);
			break;
		case 'n':
			board[destination.file][destination.rank]._piece = new knight(player_to_move, destination.file, destination.rank, true);
			break;
		default:
			std::cout << "Error: invalid input. Try again:" << std::endl;
			b = true;
			break;
		}
	}
	return res;
}
//bool checking???????
bool castling_move(Position source, Position destination) {
	return true;
}
//bool checking???????
bool check_checkmate() {//провер€ть перед ходом return true if king is under checkmate
	if (check_king()) return false;
	Position king_position = find_king();
	mark_attacked();
	board[king_position.file][king_position.rank]._piece->moves.clear();
	board[king_position.file][king_position.rank]._piece->find(false);
	for (auto i = 0; i < board[king_position.file][king_position.rank]._piece->moves.amount(); ++i) {
		Position j = board[king_position.file][king_position.rank]._piece->moves[i].destination;
		if (!board[j.file][j.rank].is_attacked()) {
			unmark_attacked();
			return false;
		}
	}
	unmark_attacked();
	if (checking_pieces.amount() > 1) return true;
	Position checking_piece_position = checking_pieces[0].position;
	if (checking_pieces[0].piece_type == KNIGHT) {
		for (auto i = 0; i < 8; ++i) {
			for (auto j = 0; j < 8; ++j) {
				if (board[i][j].color() == player_to_move) {
					board[i][j]._piece->moves.clear();
					board[i][j]._piece->find(false);
					for (auto k = 0; k < board[i][j]._piece->moves.amount(); ++k) {
						if (board[i][j]._piece->moves[k].destination == checking_piece_position) {
							if (capture_move(board[i][j]._piece->moves[k].destination, checking_piece_position, true)) return false;
						}
					}
				}
			}
		}
	}
	if (checking_pieces[0].piece_type == PAWN) {
		for (auto i = 0; i < 8; ++i) {
			for (auto j = 0; j < 8; ++j) {
				if (board[i][j].color() == player_to_move) {
					board[i][j]._piece->moves.clear();
					board[i][j]._piece->find(false);
					for (auto k = 0; k < board[i][j]._piece->moves.amount(); ++k) {
						if (board[i][j]._piece->moves[k].destination == checking_piece_position) {
							if (board[i][j]._piece->moves[k].move_type == CAPTURE) {
								if (capture_move(board[i][j]._piece->moves[k].destination, checking_piece_position, true)) return false;
							}
							if (board[i][j]._piece->moves[k].move_type == EN_PASSANT) {
								if (en_passant_move(board[i][j]._piece->moves[k].destination, checking_piece_position, true)) return false;
							}
						}
					}
				}
			}
		}
	}
	if (checking_pieces[0].piece_type == QUEEN) {
		for (auto i = 0; i < 8; ++i) {
			for (auto j = 0; j < 8; ++j) {
				if (board[i][j].color() == player_to_move) { //ƒќЅј¬»“№ ѕ–ќ¬≈– ” ЅЋќ ј
					board[i][j]._piece->moves.clear();
					board[i][j]._piece->find(false);
					for (auto k = 0; k < board[i][j]._piece->moves.amount(); ++k) {
						if (board[i][j]._piece->moves[k].destination == checking_piece_position) {
							if (capture_move(board[i][j]._piece->moves[k].destination, checking_piece_position, true)) return false;
						}
					}
					if (king_position.file == i) {
						auto diff = king_position.rank - j;
						if (diff > 1) {
							for (auto k = 0; k < board[i][j]._piece->moves.amount(); ++k) {
								for (auto q = 1; q < diff; ++q) {
									if (board[i][j]._piece->moves[k].destination == Position(i, king_position.rank + q)) {
										move_types MT = board[i][j]._piece->moves[k].move_type;
										bool successful;
										switch (MT) {
											//здесь можно без long_pawn_move
										}
									}
								}
							}
						}
						if (diff < -1) {
							for (auto k = 0; k < board[i][j]._piece->moves.amount(); ++k) {
								for (auto q = -1; q > diff; --q) {
									if (board[i][j]._piece->moves[k].destination == Position(i, king_position.rank + q)) {
										move_types MT = board[i][j]._piece->moves[k].move_type;
										bool successful;
										switch (MT) {
											//здесь можно без long_pawn_move
										}
									}
								}
							}
						}
					}
					if (king_position.rank == j) {
						auto diff = king_position.file - i;
						if (diff > 1) {

						}
						if (diff < -1) {

						}
					}
				}
			}
		}

	}
	return true;
}

/*void initialize_rank() {
	//добавить аргументы
}

void initialize_settings(char* str) {

}

void read_PGN(char* PGN) {//сделать через std::string
	char rank[9];
	char* temp = PGN;
	int cnt = 0;
	for (auto i = 7; i >= 0; i--) {
		while (*temp != '/') {
			if (cnt > 8) throw "Error: invalid PGN";
			rank[cnt] = *temp;
			++cnt;
			++temp;
		}
		++temp;
		rank[cnt + 1] = '/';
		cnt = 0;
		initialize_rank(rank, i);
	}
	while (*temp != ' ') {
		if (cnt > 8) throw "Error: invalid PGN";
		rank[cnt] = *temp;
		++cnt;
		++temp;
	}
	++temp;
	rank[cnt + 1] = '/';
	cnt = 0;
	initialize_rank(rank, 0);
	while (*temp != '\0') {
		if (*temp == ' ') {
			++temp;
			continue;
		}
		rank[cnt] = *temp;
		++cnt;
		++temp;
	}
	initialize_settings(rank);
}*/

void resignation(bool player) {
	player ? game_result.result = results::WHITE_WINS : game_result.result = results::BLACK_WINS;
	game_result.cause = causes::RESIGNATION;
}

bool check_move(char* move) {
	if (!strcmp(move, "res")) return true;
	if (move[0] < 'a' || move[0] > 'h') return false;
	if (move[1] < '1' || move[1] > '8') return false;
	if (move[2] < 'a' || move[2] > 'h') return false;
	if (move[3] < '1' || move[3] > '8') return false;
}

void make_move() {
	std::cout << "Enter your move as 4-character string (for instance, e2e4):" << std::endl;
	char move[5];
	std::cin >> move;
	if (!strcmp(move, "res")) {
		resignation(player_to_move);
		return;
	}
	if (!check_move(move)) {
		std::cout << "Error: invalid input" << std::endl;
		return;
	}
	Position source(static_cast<short>(move[0] - 97), static_cast<short>(move[1] - 49));
	Position destination(static_cast<short>(move[2] - 97), static_cast<short>(move[3] - 49));
	if (!board[source.file][source.rank].occupied()) {
		std::cout << "Error: source square does not contain a piece" << std::endl;
		return;
	}
	if (board[source.file][source.rank].color() != player_to_move) {
		std::cout << "Error: piece chosen belongs to other player" << std::endl;
		return;
	}

	board[source.file][source.rank]._piece->moves.clear();
	board[source.file][source.rank]._piece->find(false);
	bool possible = false;
	unsigned short i = 0;
	for (; i < board[source.file][source.rank]._piece->moves.amount(); ++i) {
		if (board[source.file][source.rank]._piece->moves[i].destination == destination) {
			possible = true;
			break;
		}
	}
	if (!possible) {
		std::cout << "Error: impossible move" << std::endl;
		return;
	}

	move_types move_type = board[source.file][source.rank]._piece->moves[i].move_type;
	if (en_passant == true) {
		en_passant_cnt++;
		if (en_passant_cnt > 1) {
			en_passant = false;
			en_passant_cnt = 0;
		}
	}
	bool successful = false;
	switch (move_type) {
	case NO_CAPTURE:
		successful = no_capture_move(source, destination);
		break;
	case CAPTURE:
		successful = capture_move(source, destination);
		break;
	case PROMOTION:
		successful = promotion(source, destination);
		break;
	case CAPTURE_WITH_PROMOTION:
		successful = promotion(source, destination, true);
		break;
	case EN_PASSANT:
		successful = en_passant_move(source, destination);
		en_passant = false;
		break;
	case LONG_PAWN_MOVE:
		successful = no_capture_move(source, destination);
		en_passant = true;
		en_passant_position.file = source.file;
		player_to_move ? en_passant_position.rank = source.rank - 1 : en_passant_position.rank = source.rank + 1;
	case CASTLES://дописать ли надо?
		successful = castling_move(source, destination);
		break;
	}

	if (successful) {
		if (board[destination.file][destination.rank].piece_type() == ROOK) {
			if (player_to_move) {
				if (source.file == 0) castle[3] = false;
				if (source.file == 7) castle[2] = false;
			}
			else {
				if (source.file == 0) castle[1] = false;
				if (source.file == 7) castle[0] = false;
			}
		}
		player_to_move = !player_to_move;
	}
	return;
}

void declare_result() {
	switch (game_result.cause) {
	case causes::CHECKMATE:
		std::cout << "Checkmate." << std::endl;
		break;
	case causes::RESIGNATION:
		player_to_move ? std::cout << "Black " : std::cout << "White ";
		std::cout << "resigned." << std::endl;
		break;
	case causes::AGREEMENT_TO_A_DRAW:
		std::cout << "A draw was agreed upon." << std::endl;
		break;
	case causes::UNSUFFICIENT_MATERIAL:
		std::cout << "Unsufficient material." << std::endl;
		break;
	case causes::WHITE_OUT_OF_TIME:
		std::cout << "White is out of time." << std::endl;
		break;
	case causes::BLACK_OUT_OF_TIME:
		std::cout << "Black is out of time." << std::endl;
		break;
	case causes::THREEFOLD_REPETITION:
		std::cout << "Threefold repetition has been declared." << std::endl;
		break;
	case causes::FIVEFOLD_REPETITION:
		std::cout << "Fivefold repetition has been reached." << std::endl;
		break;
	case causes::BY_50_MOVE_RULE:
		std::cout << "50 move rule has been declared." << std::endl;
		break;
	case causes::BY_75_MOVE_RULE:
		std::cout << "75 move rule has been reached." << std::endl;
		break;
	default:
		std::cout << "The game is still in progress" << std::endl;
		return;
		break;
	}
	switch (game_result.result) {
	case results::DRAW:
		std::cout << "Draw." << std::endl;
		break;
	case results::WHITE_WINS:
		std::cout << "White has won the game." << std::endl;
		break;
	case results::BLACK_WINS:
		std::cout << "Black has won the game." << std::endl;
		break;
	default:
		std::cout << "The game is still in progress" << std::endl;
		return;
		break;
	}
}

int main() {
	srand(static_cast<int>(time(NULL)));
	game_type = game_types::CLASSIC;
	initialize_board();

	while(game_result.result == results::GAME_IN_PROGRESS) {
		system("CLS");
		print_board();
		if (check_checkmate()) {
			std::cout << "Checkmate" << std::endl;
			//checkmate(player_to_move);
			break;
		}
		make_move();
	}

	system("CLS");
	print_board();
	declare_result();
	delete_board();
	system("PAUSE");

	return 0;
}