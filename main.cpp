#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    static regex include_1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex include_2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    smatch m;
    string line;
    int i = 1;
    bool error = true;

    ifstream cin_file(in_file);
    if (!cin_file.is_open()) {
        return false;
    }

    ofstream cout_file(out_file, ios::app);
    if (!cout_file.is_open()) {
        return false;
    }

    while (getline(cin_file, line) && error) {
        if (regex_match(line, m, include_1)) {
            path new_path = in_file.parent_path() / m[1].str();
            error = Preprocess(new_path, out_file, include_directories);
            if (error) {
                ++i;
                continue;
            }
            if (!filesystem::exists(new_path)) {
                for (const auto& dir_entry : include_directories) {
                    path new_path = dir_entry / m[1].str();
                    if (filesystem::exists(new_path)) {
                        error = Preprocess(new_path, out_file, include_directories);
                        break;
                    }
                }
                if (error) {
                    ++i;
                    continue;
                }
                cout << "unknown include file " << m[1].str() << " at file " << in_file.string() << " at line " << i << endl;
                return false;
            }
        }
        else if (regex_match(line, m, include_2)) {
            for (const auto& dir_entry : include_directories) {
                path new_path = dir_entry / m[1].str();
                if (filesystem::exists(new_path)) {
                    error = Preprocess(new_path, out_file, include_directories);
                    break;
                }
                cout << "unknown include file " << m[1].str() << " at file " << in_file.string() << " at line " << i << endl;
                return false;
            }
            continue;
        }
        else if (!cin_file) {
            cout << "unknown include file " << m[1].str() << " at file " << in_file.string() << " at line " << i << endl;
            return false;
        }
        cout_file << line << endl;
        ++i;
    }
    return true;

}

string GetFileContents(string file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}