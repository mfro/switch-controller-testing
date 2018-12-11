#include "common.h"

using namespace std;

void error(const string &str)
{
    string msg = str;
    if (errno != 0)
        msg = msg + ": " + strerror(errno);

    cerr << msg << endl;
    throw runtime_error(msg);
}

std::string to_hex(usize value, int width)
{
    std::stringstream s;
    s << std::hex
      << std::setfill('0')
      << std::setw(width)
      << value;
    return s.str();
}

bool operator==(const bdaddr_t &b1, const bdaddr_t &b2)
{
    return 0 == bacmp(&b1, &b2);
}

bool operator!=(const bdaddr_t &b1, const bdaddr_t &b2)
{
    return !(b1 == b2);
}
