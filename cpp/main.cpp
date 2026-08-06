#include "eval.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char **argv)
{
    std::string text;

    if (argc > 1)
    {
        std::ifstream file(argv[1]);
        if (!file)
        {
            std::cerr << "error: cannot open file: " << argv[1] << std::endl;
            return 1;
        }
        std::stringstream buf;
        buf << file.rdbuf();
        text = buf.str();
    }
    else
    {
        std::stringstream buf;
        buf << std::cin.rdbuf();
        text = buf.str();
    }

    auto tokens = tokenize(text);
    if (tokens.empty())
        return 0;

    auto ast = parse(tokens);
    auto env = make_global_env();
    try
    {
        auto result = eval_expr(ast, env);
        if (result)
        {
            if (result->is_int())
                std::cout << result->int_val << std::endl;
            else if (result->is_float())
                std::cout << result->float_val << std::endl;
            else if (result->is_bool())
                std::cout << (result->bool_val ? "true" : "false") << std::endl;
            else if (result->is_text())
                std::cout << result->text_val << std::endl;
            else if (result->is_nil())
                std::cout << "null" << std::endl;
            else if (result->is_lambda())
                std::cout << "<lambda>" << std::endl;
            else if (result->is_builtin())
                std::cout << "<builtin>" << std::endl;
            else if (result->is_symbol())
                std::cout << "Symbol(" << result->symbol_val->name << ")" << std::endl;
            else
                std::cout << "<value>" << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}