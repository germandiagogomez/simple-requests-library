#include "gdg/srl/srl.hpp"
#include "gdg/srl/exceptions.hpp"
#include <iostream>
#include <cassert>

namespace srl = gdg::srl;
using namespace std;

int main() {
    try {
        auto response =
            srl::async_http_get(
                 "www.boost.org",
                 "/LICENSE_1_0.txt"
                 , chrono::seconds(5)      //Optionally you can specify a timeout
                 /*, my_loop */ //and an io_service
                 );

        auto r = response.get();
        assert(r.first == 200);

        for (char c : r.second)
            cout << c;
        cout << endl;
    }
    catch (srl::timeout_exception) {
        cout << "Timeout";
        return -1;
    }
}
