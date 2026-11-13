#include <iostream>
#include "kaisou/core/log.hpp"

int main() {
    KAISOU_LOG_INITIALIZE();
    KAISOU_LOG_INFO("main", "kaisou started");
    KAISOU_LOG_SHUTDOWN();
    return 0;
}