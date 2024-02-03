// LC3 Decompiler. Written by Charles Cowdery
//

#include <iostream>
#include <bitset>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include<iomanip>
#include <map>

using namespace std;

uint16_t base_pc;
uint16_t offset;

template <typename t> string N2BS(t param) {
    return std::bitset< 64 >(param).to_string();
}

template <typename t> string N2H(t param,int len = 0, bool prefix = true) {
    stringstream buf;
    buf << hex << param;
    string out = buf.str();
    while (out.size() < len) {
        out = "0" + out;
    }
    if(prefix) out = "x" + out;
    return out;
}

template <typename t> t BS2B(string param) {
    t out = 0;
    for (int i = 0; i < param.size(); i++) {
        bool res = param[i] == '1';
        out = out << 1;
        out = out | res;
    }
    return out;
}

template <typename t> vector<char> B2BV(t param) {
    int width = sizeof(param);
    vector<char> out;
    for (int i = 0; i < width; i++) {
        out.push_back(param&1);
        param = param >> 1;
    }
    return out;
}

template <typename t> t create_mask(int width) {
    t out = 0;
    for (int i = 0; i < width; i++) {
        out = out | 1;
        out = out << 1;
    }
    return out >> 1;
}

#define OP_MASK 0xf000
#define P_MASK 0x0fff

int signbit_mask(int width) {
    return (1 << (width - 1));
}
bool _bit_ispos(uint16_t param, int width) {
    return (param & signbit_mask(width)) == 0;
}
int _bit_uval(uint16_t param, int width) {
    return (param & ~signbit_mask(width));
}
int _bit_val(uint16_t param, int width) {
    bool sign = _bit_ispos(param, width);
    if (sign) {
        return _bit_uval(param, width);
    }
    else {
        return (((~param) & create_mask<uint16_t>(width - 1)) + 1);
    }
}
int _bit_sval(uint16_t param, int width) {
    bool sign = _bit_ispos(param,width);
    if (sign) {
        return _bit_uval(param,width);
    }
    else {
        return -_bit_val(param,width);
    }
}
int extend_sign(uint16_t param, int width) {
    if (!_bit_ispos(param, width)) {
        param = 0xffff - _bit_val(param, 5);
    }
    return param;
}


struct op_data {
    static const vector<string> names;
    static const vector<vector<uint16_t>> param_offsets;

    uint16_t PC=0;
    uint16_t raw;
    string raw_str;
    vector<char> raw_BV;

    char raw_op_code;
    uint16_t raw_op_param;

    string op_code;
    vector<uint16_t> params;

    op_data(string op) {
        raw_str = op;
        raw = BS2B<uint16_t>(op);
        raw_BV = B2BV(raw);
        
        raw_op_code = (raw & OP_MASK) >> 12;
        raw_op_param = raw & P_MASK;

        op_code = names[raw_op_code];

        vector<uint16_t> offsets = param_offsets[raw_op_code];
        int dist = 12;
        for (auto offset : offsets) {
            dist -= offset;
            uint16_t mask = create_mask<uint16_t>(offset) << dist;
            uint16_t data = (raw_op_param & mask) >> dist;
            params.push_back(data);
        }
    }
    string to_str(bool pretty) {
        stringstream buf;
        to_SS(buf, pretty);
        return buf.str();
    }
    void to_SS(stringstream& SS,bool pretty) {
        if (op_code == "ADD") {
            if (params[2]) {
                if (pretty) {
                    if (_bit_ispos(params[3],5)) {
                        SS << "r" << params[0] << " = " << "r" << params[1] << " + " << _bit_val(params[3],5);
                    }
                    else {
                        SS << "r" << params[0] << " = " << "r" << params[1] << " - " << _bit_val(params[3],5);
                    }
                }
                else {
                    SS << "ADD r" << params[0] << " r" << params[1] << " " << _bit_sval(params[3],5);
                }
            }
            else {
                if (pretty) {
                    SS << "r" << params[0] << " = " << "r" << params[1] << " + " << "r" << params[3];
                }
                else {
                    SS << "ADD r" << params[0] << " r" << params[1] << " r" << params[3];
                }
            }
        }
        if (op_code == "AND") {
            if (params[2]) {
                if (pretty) {
                    int16_t num = (int16_t)params[3];
                    
                    num = extend_sign(num,5);
                    string hex = N2H(num, 4);
                    if ((int16_t)params[3] > 0) {
                        SS << "r" << params[0] << " = " << "r" << params[1] << " & " << hex;
                    }
                    else {
                        SS << "r" << params[0] << " = " << "r" << params[1] << " & " << hex;
                    }
                }
                else {
                    SS << "AND r" << params[0] << " r" << params[1] << " " << (int16_t)params[3];
                }
            }
            else {
                if (pretty) {
                    SS << "r" << params[0] << " = " << "r" << params[1] << " & " << "r" << params[3];
                }
            }
        }
        if (op_code == "ST") {
            int num = _bit_sval(params[1], 9);
            string rel = "*M" + N2H(base_pc+PC + num, 4) + " (" + to_string(num+PC+offset) + ")";
            if (pretty) {
                SS << rel << " = " << "r" << params[0];
            }
            else {
                SS << "ST " << rel << " " << "r" << params[0];
            }
        }
        if (op_code == "LEA") {
            int num = _bit_sval(params[1], 9) + 1;
            string rel = "M" + N2H(base_pc+PC + num, 4) + " (" + to_string(num+PC+offset)+")";
            if (pretty) {
                SS << "r" << params[0] << " = " << rel;
            }
        }
        if (op_code == "LDR") {
            string dst = "r" + to_string(params[0]);
            string base = "r" + to_string(params[1]);
            int rel =  _bit_sval(params[2], 6);
            if (pretty) {
                if (rel >= 0) {
                    SS << dst << " = *M(" << base << " + " << rel << ")";
                }
                else {
                    SS << dst << " = *M(" << base << " - " << -rel << ")";
                }
            }

        }
        if (op_code == "STR") {
            string src = "r" + to_string(params[0]);
            string base = "r" + to_string(params[1]);
            int rel = _bit_sval(params[2], 6);
            if (pretty) {
                if (rel >= 0) {
                    SS << "*M(" << base << " + " << rel << ") = " + src;
                }
                else {
                    SS << "*M(" << base << " - " << -rel << ") = " + src;
                }
            }

        }
        if (op_code == "BR") {
            if (pretty) {
                string cond = "";
                vector<char> conds = { 'N','Z','P'};
                for (int i = 0; i < 3; i++) {
                    if (params[i]) {
                        if (cond.size() > 0)cond += "|";
                        cond += conds[i];
                    }
                }
                int num = _bit_sval(params[3], 9) + 1;
                string rel = "*M" + N2H(base_pc + PC + num, 4) + " (" + to_string(num + PC + offset) + ")";
                if (cond == "N|Z|P") {
                    SS << "JUMP " << rel;
                }
                else {
                    SS << "if(" << cond << ") jump " << rel;
                }
            }
        }
        if (op_code == "TRAP") {
            if (pretty) {
                if (params[1] == 0x20) {
                    SS << "GETC";
                }
                if (params[1] == 0x21) {
                    SS << "ECHO";
                }
                if (params[1] == 0x22) {
                    SS << "PUTS";
                }
                if (params[1] == 0x23) {
                    SS << "IN";
                }
                
                if (params[1] == 0x25) {
                    SS << "HALT";
                }
            }
        }
        if (op_code == "LD") {
            int num = _bit_sval(params[1], 9);
            string rel = "*M" + N2H(base_pc + PC + num, 4) + " (" + to_string(num + PC + offset) + ")";
            if (pretty) {
                SS << "r" << params[0] << " = " << rel ;
            }
        }
        if (op_code == "NOT") {
            if (pretty) {
                SS << "r" << params[0] << " = ~r" << params[1];
            }
        }
    }
};

const vector<string> op_data::names = {
    "BR",
    "ADD",
    "LD",
    "ST",
    "JSRR",
    "AND",
    "LDR",
    "STR",
    "RTI",
    "NOT",
    "LDI",
    "STI",
    "JMP",
    "*-*",
    "LEA",
    "TRAP"
};

const vector < vector<uint16_t>> op_data::param_offsets = {
    {1,1,1,9},
    {3,3,1,5},
    {3,9},
    {3,9},
    {1,2,3,6},
    {3,3,1,5},
    {3,3,6},
    {3,3,6},
    {12},
    {3,3,6},
    {3,9},
    {3,9},
    {3,3,6},
    {12},
    {3,9},
    {4,8}
};

vector<op_data> program;

map<string, string> escapes{
    {"\n","\\n"},
    {"\t","\\t"},
    {"\v","\\v"},
    {" ","\" \""},
    {"\r","\\r"},
    {"\f","\\f"},
    {"\b","\\b"},
    {"\a","\\a"},
};

string remap_escapes(string in) {
    for (auto c_pair : escapes) {
        if (in == c_pair.first) {
            return c_pair.second;
        }
    }
    return in;
}

int main()
{
    cout << "enter file name" << endl;
    string file_name;
    getline(cin, file_name);
    ifstream file(file_name);
    stringstream buffer;
    buffer << file.rdbuf();
    int i = 0;
    base_pc = 0x3000;
    offset = 1;
    while (!buffer.eof()) {
        string data;
        getline(buffer, data);
        if (data.size() < 16) break;
        string rel_data = data.substr(0, 16);
        string comment = data.substr(16,data.size());
        op_data op(rel_data);
        op.PC = i;

        string c = "";
        char c_raw = ((char)(op.raw & (0x007f)));
        if (c_raw == 0) c = "\\0";
        else c += c_raw;
        c = remap_escapes(c);
        c = c;

        program.push_back(op_data(data));
        cout << right;
        cout << setw(4) << op.op_code;
        cout << setw(7) << "M" + N2H(base_pc + i, 4);
        cout << setw(4) << i + offset << " " << left << setw(30) << op.to_str(true);
        cout << setw(40) << comment;
        
        cout << setw(5) << c;
        cout << " " << op.raw_str;
        cout << endl;
        i++;

    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
