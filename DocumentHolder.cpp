// Function definition for function prototypes in DocumentHolder.hpp
#include "DocumentHolder.hpp"
#include "ThreadPool.hpp"
#include <regex>
#include <thread>
#include <iostream>
namespace para {
    line::line(const std::string& text) : data(text),locker() {}
    cursor_t::cursor_t(std::list<line>::iterator _lineptr,unsigned _location) : line_ptr(_lineptr),location(_location) {}
    user::user(std::list<line>::iterator _lineptr) : cursor(_lineptr,0) {}
    void document::print_to_file(std::ostream& output) {            // function to print to output stream
        for (const auto& line: lines) {
            auto temp = line.data.reconstruct();
            output<<temp<<"\n";
        }
    }
    void document::shift_cursor(user& use,cursor_t new_cursor) {    // function to shift cursor to required location
        use.cursor=new_cursor;
    }
    std::vector<std::string> document::get_lines(user& use,unsigned lower,unsigned upper) { // private function that reconstructs the entire document and returns a vector of strings
        auto iter=lines.begin();
        std::vector<std::string> retval;
        if (lower>=lines.size()) {
            lower=lines.size();
        }
        if (upper>lines.size()) {
            upper=lines.size();
        }
        for (unsigned i=0;i!=lower;++i) {
            ++iter;
        }
        for (unsigned i=lower;i!=upper;++i,++iter) {
            std::lock_guard<std::mutex> lock(iter->locker);
            retval.emplace_back(iter->data.reconstruct());
        }
        return retval;
    }
    void document::append_after(user& use,const std::string& text) {                // function to append a string to a line
        const auto lineptr=use.cursor.line_ptr;
        std::lock_guard<std::mutex> lock(lineptr->locker);
        auto& buf=lineptr->data;
        buf.absolute_move(buf.length());
        buf.insert(text);
    }
    void document::replace_line(user& use,const std::string& text) {                // function to replace a string on a given line
        const auto lineptr=use.cursor.line_ptr;
        std::lock_guard<std::mutex> lock(lineptr->locker);
        auto& buf=lineptr->data;
        buf.replace(text);
    }
    void document::insert_before(user& use,const std::string& text) {               // function to insert string at the beginning of the line
        const auto lineptr=use.cursor.line_ptr;
        std::lock_guard<std::mutex> lock(lineptr->locker);
        auto& buf =lineptr->data;
        buf.absolute_move(0);
        buf.insert(text);
    }
    void document::remove_in_line(user& use,int offset) {                           // function to remove a set of characters from the position of the cursor in a line
        const auto lineptr=use.cursor.line_ptr;
        std::lock_guard<std::mutex> lock(lineptr->locker);
        auto& buf=lineptr->data;
        if (int{use.cursor.location}+offset>=int{buf.length()}) {
            offset=int{buf.length()}-int{use.cursor.location};
        }
        if (int{use.cursor.location}+offset<=0) {
            offset=-int{use.cursor.location};
        }
        buf.absolute_move(use.cursor.location);
        buf.remove(offset);
    }
    document::document(const std::vector<std::string>& _lines) : lines(),users() {
        for (const auto& str : _lines) {
            lines.emplace_back(str);
        }
    }

    // regular expressions for filtering the commands appropriately
    std::regex user_join(R"(^join (\d+)$)");
    std::regex user_leave(R"(^leave (\d+)$)");
    std::regex list_cmd(R"(^l\((\d+),(\d+)\)$)");
    std::regex shift_cmd(R"(^(\d+)(h|j|k|l)$)");
    std::regex append_cmd(R"(^a\((.*)\)$)");
    std::regex insert_cmd(R"(^i\((.*)\)$)");
    std::regex replace_cmd(R"(^r\((.*)\)$)");
    std::regex delete_cmd(R"(^d\((-?)(\d+)\)$)");
    void document::process(unsigned uid,const std::string& text,std::ostream& output) { // processor function to perform the correct operations in the document holder
        if (uid==0) { //message from server
            std::smatch matches;
            if (std::regex_match(text,matches,user_join)) {
                users.emplace(std::make_pair(std::stoi(matches.str(1)),user(lines.begin())));
            }
            else if (std::regex_match(text,matches,user_leave)) { //a user is closing the document
                users.erase(std::stoi(matches.str(1)));
            }
        }
        else {
            const unsigned real_uid=uid;
            auto useptr=users.find(real_uid);
            auto& use=useptr->second;
            std::smatch matches;
            if (std::regex_match(text,matches,shift_cmd)) {         // function to shift the cursor
                auto match1=matches.str(1);
                auto match2=matches.str(2);
                auto task=[this,match1,match2,&use](){
                    cursor_t new_cursor(use.cursor);
                    auto count=std::stoul(match1);
                    if (match2=="h") {                              // left movement within a line
                        if (new_cursor.location>count) {
                            new_cursor.location-=count;
                        }
                        else {
                            new_cursor.location=0;
                        }
                    }
                    else if (match2=="j") {                         // moving downwards in a set of lines
                        for (unsigned i=0;i!=count;++i) {
                            auto temp=new_cursor.line_ptr;
                            ++temp;
                            if (temp!=lines.end()) {
                                new_cursor.line_ptr=temp;
                            }
                        }
                    }
                    else if (match2=="k") {                         // moving upwards in a set of lines
                        for (unsigned i=0;i!=count;++i) {
                            if (new_cursor.line_ptr!=lines.begin()) {
                                --new_cursor.line_ptr;
                            }
                        }
                    }
                    else {                                          // moving right within the lines
                        if (new_cursor.location+count>new_cursor.line_ptr->data.length()) {
                            new_cursor.location=new_cursor.line_ptr->data.length();
                        }
                        else {
                            new_cursor.location+=count;
                        }
                    }
                    shift_cursor(use,new_cursor);                   // shift after calculating the offset
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
            else if (std::regex_match(text,matches,list_cmd)) {     // function to list the lines per request
                auto match1=matches.str(1);
                auto match2=matches.str(2);
                auto task=[this,match1,match2,&output,&use,real_uid](){
                    auto result=get_lines(use,std::stoi(match1),std::stoi(match2));
                    auto uid_str=std::to_string(real_uid);
                    std::string response;
                    for (const auto& line : result) {
                        response+=uid_str;
                        response+=" ";
                        response+=line;
                        response+="\n";
                    }
                    output<<response;
                    output.flush();
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
            else if (std::regex_match(text,matches,append_cmd)) {   // user side function to append to a line
                auto match1=matches.str(1);
                auto task=[this,match1,&use](){
                    append_after(use,match1);
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
            else if (std::regex_match(text,matches,insert_cmd)) {   // user side function to insert to a line
                auto match1=matches.str(1);
                auto task=[this,match1,&use](){
                    insert_before(use,match1);
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
            else if (std::regex_match(text,matches,replace_cmd)) {  // user side function to replace content in a line
                auto match1=matches.str(1);
                auto task=[this,match1,&use](){
                    replace_line(use,match1);
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
            else if (std::regex_match(text,matches,delete_cmd)) {   // user side function to delete characters in a line
                auto match1=matches.str(1);
                std::string match2;
                if(matches.size()==3) {
                    match2=matches.str(2);
                }
                auto task=[this,match1,match2,&use](){
                    if (match1=="-") {
                        remove_in_line(use,-std::stoi(match2));
                    }
                    else {
                        remove_in_line(use,std::stoi(match2));
                    }
                };
                para::schedule(task);                               // schedule the task in the threadpool
            }
        }
    }
}
