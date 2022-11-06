#include "ucicommands.h"

namespace uciCommand
{

void uciInput(uciOptions options)
{
    std::cout << "id name " << uciCommand::getVersion() << std::endl;
    std::cout << "id author Disservin\n" << std::endl;
    options.printOptions();
    std::cout << "uciok" << std::endl;
}

void isreadyInput()
{
    std::cout << "readyok" << std::endl;
}

void ucinewgameInput(uciOptions &options, Board &board, Search &searcher, Datagen::TrainingData &dg)
{
    options.uciPosition(board);
    stopThreads(searcher, dg);
    searcher.tds.clear();
    clearTT();
}

void parseInput(std::string input, Board &board)
{
    if (input == "print")
    {
        board.print();
    }

    else if (input == "captures")
    {
        Movelist moves;
        Movegen::legalmoves<Movetype::CAPTURE>(board, moves);
        for (int i = 0; i < moves.size; i++)
            std::cout << uciRep(moves[i].move) << std::endl;
        std::cout << "count: " << signed(moves.size) << std::endl;
    }
    else if (input == "checks")
    {
        Movelist moves;
        Movegen::legalmoves<Movetype::CHECK>(board, moves);
        for (int i = 0; i < moves.size; i++)
            std::cout << uciRep(moves[i].move) << std::endl;
        std::cout << "count: " << signed(moves.size) << std::endl;
    }
    else if (input == "moves")
    {
        Movelist moves;
        Movegen::legalmoves<Movetype::ALL>(board, moves);
        for (int i = 0; i < moves.size; i++)
            std::cout << uciRep(moves[i].move) << std::endl;
        std::cout << "count: " << signed(moves.size) << std::endl;
    }

    else if (input == "rep")
    {
        std::cout << board.isRepetition(3) << std::endl;
    }

    else if (input == "eval")
    {
        std::cout << Eval::evaluation(board) << std::endl;
    }

    else if (input == "perft")
    {
        Perft perft = Perft();
        perft.board = board;
        perft.testAllPos();
    }
}

void stopThreads(Search &searcher, Datagen::TrainingData &dg)
{
    stopped = true;
    UCI_FORCE_STOP = true;

    for (std::thread &th : searcher.threads)
    {
        if (th.joinable())
            th.join();
    }

    for (std::thread &th : dg.threads)
    {
        if (th.joinable())
            th.join();
    }

    searcher.threads.clear();
    dg.threads.clear();

    stopped = false;
}

void quit(Search &searcher, Datagen::TrainingData &dg)
{
    stopThreads(searcher, dg);
    free(TTable);
    tb_free();
}

bool elementInVector(std::string el, std::vector<std::string> tokens)
{
    return std::find(tokens.begin(), tokens.end(), el) != tokens.end();
}

bool stringContain(std::string s, std::string origin)
{
    return origin.find(s) != std::string::npos;
}

const std::string getVersion()
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

    ss << "Smallbrain " << version << " ";

    date >> month >> day >> year;
    if (day.length() == 1)
        day = "0" + day;
    ss << year.substr(2) << months[month] << day;

#ifdef SHA
    ss << "-" << SHA;
#endif

    return ss.str();
}

} // namespace uciCommand