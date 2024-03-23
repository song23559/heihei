#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <set>
#include <memory>
#include <algorithm>

class QueryResult; // 为了定义函数query的返回类型，这个定义是必需的
using line_no = std::vector<std::string>::size_type;
class TextQuery
{
public:
    using line_no = std::vector<std::string>::size_type;
    TextQuery(std::ifstream &is); // 构造函数
    QueryResult query(const std::string &s) const;
    static void formatFile(const std::string &filePath);

private:
    std::shared_ptr<std::vector<std::string>> file; // 输入文件
    // 每个单词到它所在的行号的集合的映射
    std::map<std::string, std::shared_ptr<std::set<line_no>>> wm;
};

TextQuery::TextQuery(std::ifstream &is) : file(new std::vector<std::string>)
{
    std::string text;
    while (std::getline(is, text))
    {                                  // 对文件中每一行
        file->push_back(text);         // 保存此行文本
        int n = file->size() - 1;      // 当前行号
        std::istringstream line(text); // 将行文本分解为单词
        std::string word;
        while (line >> word)
        { // 对行中每个单词
            // 如果单词不在wm中，以之为下标在wm中添加一项
            auto &lines = wm[word];                 // lines是一个shared_ptr
            if (!lines)                             // 在我们第一次遇到这个单词时，此指针为空
                lines.reset(new std::set<line_no>); // 分配一个新的set
            lines->insert(n);                       // 将此行号插入set中
        }
    }
}

void TextQuery::formatFile(const std::string &filePath)
{
    std::ifstream inFile(filePath);
    // 打开失败输出错误信息
    if (!inFile)
    {
        std::cout << "Unable to open file!\n";
        return;
    }

    int n;
    std::cout << "Please enter the maximum number of characters per line:";
    std::cin >> n;

    // 存储新文件路径
    std::string outFilePath;
    while (true)
    {
        std::cout << "Please enter the location and filename for the new file:";
        std::cin >> outFilePath;

        std::ifstream testFile(outFilePath);

        // 新文件存在则提醒用户是否替换
        if (testFile)
        {
            std::cout << "The file already exists. Do you want to replace it? (y/n): ";
            char answer;
            std::cin >> answer;
            if (answer == 'y' || answer == 'Y')
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    std::ofstream outFile(outFilePath);
    if (!outFile)
    {
        std::cout << "Unable to create new file!\n";
        return;
    }

    std::string line, word;

    while (std::getline(inFile, line))
    {
        std::istringstream iss(line);
        int len = 0;

        // 读取一个单词存入word变量
        while (iss >> word)
        {
            // 长度超过n则换行
            if (len + word.length() > n)
            {
                outFile << "\n";
                len = 0;
            }
            // 是一个单词则输出一个空格
            if (len > 0)
            {
                outFile << " ";
                len++;
            }
            outFile << word;
            len += word.length();
        }
        outFile << "\n";
    }

    std::cout << "File formatting completed!\n";
}

class QueryResult
{
    friend std::ostream &print(std::ostream &, const QueryResult &);

public:
    // 一个指向文件的智能指针
    std::shared_ptr<std::vector<std::string>> get_file() { return file; }
    // 指向第一个和最后一个行号的迭代器
    std::set<line_no>::iterator begin() { return lines->begin(); }
    std::set<line_no>::iterator end() { return lines->end(); }
    QueryResult(std::string s,
                std::shared_ptr<std::set<line_no>> p,
                std::shared_ptr<std::vector<std::string>> f) : sought(s), lines(p), file(f) {}

    std::string sought;                             // 查询单词
    std::shared_ptr<std::set<line_no>> lines;       // 出现的行号
    std::shared_ptr<std::vector<std::string>> file; // 输入文件
};

// 查询函数，接收一个字符串（单词），返回该单词在文件中出现的所有行号。
QueryResult
TextQuery::query(const std::string &sought) const
{
    // 如果未找到 sought，我们将返回一个指向此 set 的指针
    static std::shared_ptr<std::set<line_no>> nodata(new std::set<line_no>);
    // 使用find而不是下标运算符来查找单词，避免将单词添加到 wm 中！
    auto loc = wm.find(sought);
    if (loc == wm.end())
        return QueryResult(sought, nodata, file); // 未找到
    else
        return QueryResult(sought, loc->second, file); // 返回一个QueryResult对象
}

std::ostream &print(std::ostream &os, const QueryResult &qr)
{
    // 如果找到了单词，打印出现次数和所有出现的位置
    os << qr.sought << " occurs " << qr.lines->size() << " "
       << "times" << std::endl;
    // 打印单词出现的每一行
    for (auto num : *qr.lines) // 对 set 中每个单词
        // 避免行号从0开始给用户带来的困惑
        os << "\t(line " << num + 1 << ") "
           << *(qr.file->begin() + num) << std::endl;
    return os;
}

// 一个抽象基类，具体的查询从中派生
class Query_base
{
public:
    using line_no = TextQuery::line_no; // 用于eval函数
    virtual ~Query_base() = default;    // 默认构造函数，

    // eval返回与当前query匹配的queryresualt
    virtual QueryResult eval(const TextQuery &) const = 0;
    // rep表示查询的是一个string
    virtual std::string rep() const = 0;
};

// 一个接口类，它包含一个指向 Query_base 对象的智能指针。它提供了 eval 和 rep 方法，这两个方法分别用于执行查询和获取查询的字符串表示。
class Query
{
    // 这些运算符要接受shared_ptr构造函数
    friend Query operator~(const Query &);
    friend Query operator|(const Query &, const Query &);
    friend Query operator&(const Query &, const Query &);

public:
    Query() = default;          // 添加默认构造函数
    Query(const std::string &); // 构建新的wordquery

    // 接口函数调用对应的base
    QueryResult eval(const TextQuery &t) const
    {
        return q->eval(t);
    }
    std::string rep() const { return q->rep(); }

private:
    Query(std::shared_ptr<Query_base> query) : q(query) {} // 构造函数
    std::shared_ptr<Query_base> q;
};

class WordQuery : public Query_base
{
    friend class Query; // query使用wordquery构造函数
    WordQuery(const std::string &s) : query_word(s) {}
    // 具体类：定义继承而来的纯虚函数
    QueryResult eval(const TextQuery &t) const
    {
        return t.query(query_word);
    }
    std::string rep() const { return query_word; }
    std::string query_word;
};

class NotQuery : public Query_base
{
    friend Query operator~(const Query &);
    NotQuery(const Query &q) : query(q) {}
    // 具体类：定义继承而来的纯虚函数
    std::string rep() const { return "~(" + query.rep() + ")"; }
    QueryResult eval(const TextQuery &) const;
    Query query;
};

class BinaryQuery : public Query_base
{
protected:
    BinaryQuery(const Query &l, const Query &r, std::string s) : lhs(l), rhs(r), opSym(s) {}

    // 抽象类：不定义继承而来的纯虚函数
    std::string rep() const { return "(" + lhs.rep() + " " + opSym + " " + rhs.rep() + ")"; }
    Query lhs, rhs;    // 左侧和右侧运算对象
    std::string opSym; // 运算符名字
};

class AndQuery : public BinaryQuery
{
    friend Query operator&(const Query &, const Query &);
    AndQuery(const Query &left, const Query &right) : BinaryQuery(left, right, "&") {}
    // 具体类：定义继承而来的纯虚函数
    QueryResult eval(const TextQuery &) const;
};

class OrQuery : public BinaryQuery
{
    friend Query operator|(const Query &, const Query &);
    OrQuery(const Query &left, const Query &right) : BinaryQuery(left, right, "|") {}
    // 具体类：定义继承而来的纯虚函数
    QueryResult eval(const TextQuery &) const;
};

// 接受string 的构造函数
inline Query::Query(const std::string &s) : q(new WordQuery(s)) {}

inline Query operator~(const Query &operand)
{
    return std::shared_ptr<Query_base>(new NotQuery(operand));
}

inline Query operator|(const Query &lhs, const Query &rhs)
{
    return std::shared_ptr<Query_base>(new OrQuery(lhs, rhs));
}

inline Query operator&(const Query &lhs, const Query &rhs)
{
    return std::shared_ptr<Query_base>(new AndQuery(lhs, rhs));
}

QueryResult OrQuery::eval(const TextQuery &text) const
{
    // 对lsh和rhs的虚调用
    auto right = rhs.eval(text), left = lhs.eval(text);
    // 将左侧运算对象的行号拷贝到结果set中
    auto ret_lines =
        std::make_shared<std::set<line_no>>(left.begin(), left.end());
    // 插入右侧运算对象
    ret_lines->insert(right.begin(), right.end());
    // 返回一个新的result
    return QueryResult(rep(), ret_lines, left.get_file());
}

QueryResult NotQuery::eval(const TextQuery &text) const
{
    auto result = query.eval(text);
    auto ret_lines = std::make_shared<std::set<line_no>>();
    auto sz = result.get_file()->size();
    for (size_t n = 0; n != sz; ++n)
    {
        // 检查每一行是否在结果集中
        if (result.lines->find(n) == result.lines->end())
            ret_lines->insert(n); // 不在就添加
    }
    return QueryResult(rep(), ret_lines, result.get_file());
}

QueryResult AndQuery::eval(const TextQuery &text) const
{
    auto left = lhs.eval(text), right = rhs.eval(text);
    auto ret_lines = std::make_shared<std::set<line_no>>();

    // 用标准库求交集
    // 向迭代器中添加元素
    std::set_intersection(left.begin(), left.end(),
                          right.begin(), right.end(),
                          inserter(*ret_lines, ret_lines->begin()));
    return QueryResult(rep(), ret_lines, left.get_file());
}

std::map<std::string, Query> variables;
void runQueries()
{
    // 与用户交互：提示用户输入要查询的单词，完成查询并打印结果
    while (true)
    {
        std::cout << "Choose the function you need:\n";
        std::cout << "1. File formatting\n";
        std::cout << "2. Text query\n";
        std::cout << "3. Assign variable\n";
        std::cout << "4. Exit\n";

        int choice;
        std::cin >> choice;

        switch (choice)
        {
        case 1:
        {
            std::string filePath;
            std::cout << "Please enter file path:";
            std::cin >> filePath;
            TextQuery::formatFile(filePath);
            break;
        }
        case 2:
        {
            std::string filePath;
            std::cout << "Please enter file path:";
            std::cin >> filePath;
            std::ifstream ifs(filePath);
            if (!ifs)
            {
                std::cout << "Unable to open file!\n";
                break;
            }
            TextQuery tq(ifs);

            while (true)
            {
                std::cout << "Choose the type of query:\n";
                std::cout << "1. Normal query\n";
                std::cout << "2. Logical NOT query\n";
                std::cout << "3. Logical OR query\n";
                std::cout << "4. Logical AND query\n";
                std::cout << "5. Back to main menu\n";

                int queryType;
                std::cin >> queryType;

                if (queryType == 5)
                    break;

                std::cout << "Enter word to look for, or q to quit: ";
                std::string s;
                // 若遇到文件尾或用户输入了 'q' 时循环终止
                if (!(std::cin >> s) || s == "q")
                    break;

                // 检查输入的单词是否是一个变量名
                if (variables.count(s) > 0)
                {
                    // 如果是变量名，就使用变量的值
                    s = variables[s].rep();
                }

                // 指向查询并打印结果
                switch (queryType)
                {
                case 1:
                {
                    print(std::cout, tq.query(s)) << std::endl;
                    break;
                }
                case 2:
                {
                    Query q = Query(s);
                    Query not_q = ~q;
                    print(std::cout, not_q.eval(tq)) << std::endl;
                    break;
                }
                case 3:
                {
                    std::cout << "Enter another word: ";
                    std::string s2;
                    std::cin >> s2;
                    Query q1 = Query(s);
                    Query q2 = Query(s2);
                    Query or_q = q1 | q2;
                    print(std::cout, or_q.eval(tq)) << std::endl;
                    break;
                }
                case 4:
                {
                    std::cout << "Enter another word: ";
                    std::string s2;
                    std::cin >> s2;
                    Query q1 = Query(s);
                    Query q2 = Query(s2);
                    Query and_q = q1 & q2;
                    print(std::cout, and_q.eval(tq)) << std::endl;
                    break;
                }
                default:
                {
                    std::cout << "Invalid choice!\n";
                }
                }
            }
            break;
        }
        case 3:
        {
            std::string varName, word;
            std::cout << "Enter variable name: ";
            std::cin >> varName;
            std::cout << "Enter word: ";
            std::cin >> word;
            variables[varName] = Query(word);
            break;
        }
        case 4:
        {
            return;
        }
        default:
            std::cout << "Invalid choice!\n";
        }
    }
}

int main()
{
    runQueries();
    return 0;
}
