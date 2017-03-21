// Main driver program for Pril-ed
#include "DocumentHolder.hpp"
#include "ThreadPool.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
int main(int argc,char**argv) {
    std::ifstream reader(argv[1]);                      // ifstream object to read lines from the text file
    std::vector<std::string> lines;
    std::string temp;
    std::getline(reader,temp);
    while (reader) {
        lines.emplace_back(temp);
        std::getline(reader,temp);
    }
    reader.close();
    para::document doc(lines);                          // put the lines into the documentholder for editing
    para::start_threads();                              // start threads from the threadpool
    while (std::getline(std::cin,temp)) {               // execute for every command
        if (temp=="1 stop") {
            break;                                      // client withdraws from the server with "stop" message
        }
        auto user=temp.substr(0,temp.find(' '));        // processing the command for execution
        auto rest=temp.substr(temp.find(' ')+1,temp.size());
        doc.process(std::stoi(user),rest,std::cout);    // send command for execute
    }
    para::stop_threads();                               // on exit, stop threads in threadpool
    std::ofstream writer(argv[1]);                      // commit changes to the localcopy the client gets
    doc.print_to_file(writer);
    writer.close();
}
