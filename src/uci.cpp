#include "uci.h"

UCI::UCI()
{
    searcher = Search();
    board = Board();
    options = uciOptions();
    datagen = Datagen::TrainingData();
    useTB = false;

    threadCount = 1;

    // load default position
    board.applyFen(DEFAULT_POS);

    // Initialize reductions used in search
    init_reductions();
}

int UCI::uciLoop(int argc, char **argv)
{
    std::vector<std::string> allArgs(argv + 1, argv + argc);

    if (argc > 1 && parseArgs(argc, argv, options))
        return 0;

    // START OF TUNE

    // TUNE_INT(razorMargin, -100, 100);

    // END OF TUNE

    while (true)
    {
        // catching inputs
        std::string input;

        if (!std::getline(std::cin, input) && argc == 1)
            input = "quit";

        if (input == "quit")
        {
            quit();
            return 0;
        }
        else
            processCommand(input);
    }
}

void UCI::processCommand(std::string command)
{
    std::vector<std::string> tokens = splitInput(command);

    if (tokens[0] == "stop")
    {
        stopThreads();
    }
    else if (tokens[0] == "ucinewgame")
    {
        ucinewgameInput();
    }
    else if (tokens[0] == "uci")
    {
        uciInput();
    }
    else if (tokens[0] == "isready")
    {
        isreadyInput();
    }
    else if (tokens[0] == "setoption")
    {
        std::string option = tokens[2];
        std::string value = tokens[4];

        if (option == "Hash")
            options.uciHash(std::stoi(value));
        else if (option == "EvalFile")
            options.uciEvalFile(value);
        else if (option == "Threads")
            threadCount = options.uciThreads(std::stoi(value));
        else if (option == "SyzygyPath")
            useTB = options.uciSyzygy(command);
        else if (option == "UCI_Chess960")
            options.uciChess960(board, value);
    }
    else if (tokens[0] == "position")
    {
        bool hasMoves = elementInVector("moves", tokens);

        if (tokens[1] == "fen")
            board.applyFen(command.substr(command.find("fen") + 4), false);
        else
            board.applyFen(DEFAULT_POS, false);

        if (hasMoves)
            uciMoves(tokens);

        // setup accumulator with the correct board
        board.accumulate();
    }
    else if (command == "go perft")
    {
        int depth = findElement<int>("perft", tokens);
        Perft perft = Perft();
        perft.board = board;
        perft.perfTest(depth, depth);
    }
    else if (tokens[0] == "go")
    {
        Limits info;
        std::string limit;

        stopThreads();

        if (tokens.size() == 1)
            limit = "";
        else
            limit = tokens[1];

        info.depth = (limit == "depth") ? findElement<int>("depth", tokens) : MAX_PLY;
        info.depth = (limit == "infinite" || command == "go") ? MAX_PLY : info.depth;
        info.nodes = (limit == "nodes") ? findElement<int>("nodes", tokens) : 0;
        info.time.maximum = info.time.optimum = (limit == "movetime") ? findElement<int>("movetime", tokens) : 0;

        std::string side_str = board.sideToMove == White ? "wtime" : "btime";
        std::string inc_str = board.sideToMove == White ? "winc" : "binc";

        if (elementInVector(side_str, tokens))
        {
            int64_t timegiven = findElement<int>(side_str, tokens);
            int64_t inc = 0;
            int64_t mtg = 0;

            // Increment
            if (elementInVector(inc_str, tokens))
                inc = findElement<int>(inc_str, tokens);

            // Moves to next time control
            if (elementInVector("movestogo", tokens))
                mtg = findElement<int>("movestogo", tokens);

            // Calculate search time
            info.time = optimumTime(timegiven, inc, mtg);
        }

        // start search
        searcher.startThinking(board, threadCount, info, useTB);
    }
    else if (command == "print")
    {
        std::cout << board << std::endl;
    }
    else if (command == "captures")
    {
        Movelist moves;
        Movegen::legalmoves<Movetype::CAPTURE>(board, moves);

        for (int i = 0; i < moves.size; i++)
            std::cout << uciRep(board, moves[i].move) << std::endl;

        std::cout << "count: " << signed(moves.size) << std::endl;
    }
    else if (command == "moves")
    {
        Movelist moves;
        Movegen::legalmoves<Movetype::ALL>(board, moves);

        for (int i = 0; i < moves.size; i++)
            std::cout << uciRep(board, moves[i].move) << std::endl;

        std::cout << "count: " << signed(moves.size) << std::endl;
    }

    else if (command == "rep")
    {
        std::cout << board.isRepetition(3) << std::endl;
    }

    else if (command == "eval")
    {
        std::cout << Eval::evaluation(board) << std::endl;
    }

    else if (command == "perft")
    {
        Perft perft = Perft();
        perft.board = board;
        perft.testAllPos();
    }
    else if (contains("move", command))
    {
        if (elementInVector("move", tokens))
        {
            std::size_t index = std::find(tokens.begin(), tokens.end(), "move") - tokens.begin();
            index++;

            for (; index < tokens.size(); index++)
            {
                Move move = convertUciToMove(tokens[index]);
                board.makeMove<false>(move);
            }
        }
    }
    else
    {
        std::cout << "Unknown command: " << command << std::endl;
    }
}

bool UCI::parseArgs(int argc, char **argv, uciOptions options)
{
    std::vector<std::string> allArgs(argv + 1, argv + argc);

    // ./smallbrain bench
    if (elementInVector("bench", allArgs))
    {
        Bench::startBench();
        quit();
        return true;
    }
    else if (elementInVector("perft", allArgs))
    {
        int n = 1;
        if (elementInVector("-n", allArgs))
            n = findElement<int>("-n", allArgs);

        Perft perft = Perft();
        perft.board = board;
        perft.testAllPos(n);
        quit();
        return true;
    }
    else if (elementInVector("-gen", allArgs))
    {
        std::string bookPath = "";
        std::string tbPath = "";
        int workers = 1;
        int depth = 7;
        bool useTB = false;

        if (elementInVector("-threads", allArgs))
        {
            workers = findElement<int>("-threads", allArgs);
        }

        if (elementInVector("-book", allArgs))
        {
            bookPath = findElement<std::string>("-book", allArgs);
        }

        if (elementInVector("-tb", allArgs))
        {
            std::string s = "setoption name SyzygyPath value " + findElement<std::string>("-tb", allArgs);
            useTB = options.uciSyzygy(s);
        }

        if (elementInVector("-depth", allArgs))
        {
            depth = findElement<int>("-depth", allArgs);
        }

        datagen.generate(workers, bookPath, depth, useTB);

        std::cout << "Data generation started" << std::endl;

        return false;
    }
    else
    {
        std::cout << "Unknown argument" << std::endl;
    }
    return false;
}

void UCI::uciInput()
{
    std::cout << "id name " << getVersion() << std::endl;
    std::cout << "id author Disservin\n" << std::endl;
    options.printOptions();
    std::cout << "uciok" << std::endl;
}

void UCI::isreadyInput()
{
    std::cout << "readyok" << std::endl;
}

void UCI::ucinewgameInput()
{
    board.applyFen(DEFAULT_POS);
    stopThreads();
    searcher.tds.clear();
    TTable.clearTT();
}

void UCI::stopThreads()
{
    stopped = true;
    UCI_FORCE_STOP = true;

    for (std::thread &th : searcher.threads)
    {
        if (th.joinable())
            th.join();
    }

    for (std::thread &th : datagen.threads)
    {
        if (th.joinable())
            th.join();
    }

    searcher.threads.clear();
    datagen.threads.clear();

    stopped = false;
}

void UCI::quit()
{
    stopThreads();
    tb_free();
}

const std::string UCI::getVersion()
{
    std::unordered_map<std::string, std::string> months({{"Jan", "01"},
                                                         {"Feb", "02"},
                                                         {"Mar", "03"},
                                                         {"Apr", "04"},
                                                         {"May", "05"},
                                                         {"Jun", "06"},
                                                         {"Jul", "07"},
                                                         {"Aug", "08"},
                                                         {"Sep", "09"},
                                                         {"Oct", "10"},
                                                         {"Nov", "11"},
                                                         {"Dec", "12"}});

    std::string month, day, year;
    std::stringstream ss, date(__DATE__); // {month} {date} {year}

    const std::string version = "dev";

    ss << "Smallbrain " << version;
    ss << "-";
#ifdef GIT_DATE
    ss << GIT_DATE;
#else

    date >> month >> day >> year;
    if (day.length() == 1)
        day = "0" + day;
    ss << year.substr(2) << months[month] << day;
#endif

#ifdef GIT_SHA
    ss << "-" << GIT_SHA;
#endif

    return ss.str();
}

void UCI::uciMoves(std::vector<std::string> &tokens)
{
    std::size_t index = std::find(tokens.begin(), tokens.end(), "moves") - tokens.begin();
    index++;
    for (; index < tokens.size(); index++)
    {
        Move move = convertUciToMove(tokens[index]);
        board.makeMove<false>(move);
    }
}

Square UCI::extractSquare(std::string_view squareStr)
{
    char letter = squareStr[0];
    int file = letter - 96;
    int rank = squareStr[1] - 48;
    int index = (rank - 1) * 8 + file - 1;
    return Square(index);
}

Move UCI::convertUciToMove(std::string input)
{
    Square source = extractSquare(input.substr(0, 2));
    Square target = extractSquare(input.substr(2, 2));
    PieceType piece = type_of_piece(board.pieceAtBB(source));

    // convert to king captures rook
    if (!board.chess960 && piece == KING && square_distance(target, source) == 2)
    {
        target = file_rank_square(target > source ? FILE_H : FILE_A, square_rank(source));
    }

    switch (input.length())
    {
    case 4:
        return make(piece, source, target, false);
    case 5:
        return make(pieceToInt[input.at(4)], source, target, true);
    default:
        std::cout << "FALSE INPUT" << std::endl;
        return make(NONETYPE, NO_SQ, NO_SQ, false);
    }
}