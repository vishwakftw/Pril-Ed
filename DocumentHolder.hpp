#ifndef PARA_DOCUMENT_HOLDER_HPP
#define PARA_DOCUMENT_HOLDER_HPP
#include <array>
#include "GapBuffer.hpp"
#include <vector>
#include <mutex>
#include <list>
#include <unordered_map>
namespace para {
    constexpr const unsigned register_count=8;
    struct line {
        gap_buffer data;
        std::mutex locker;
        line(const std::string&);
    };
    struct cursor_t {
        std::list<line>::iterator line_ptr;
        unsigned location;
        cursor_t(std::list<line>::iterator,unsigned);
    };
    enum class Mode {
        Command,
        Insert
    };
    struct user { //maintains state of users
        cursor_t cursor;
        //Mode mode;
        std::array<std::string,register_count> registers;
        user(std::list<line>::iterator);
    };
    class document {
        std::list<line> lines;
        std::unordered_map<unsigned,user> users;
        void shift_cursor(user&,cursor_t);
        std::vector<std::string> get_lines(user&,unsigned,unsigned);
        void append_after(user&,const std::string&);
        void replace_line(user&,const std::string&);
        void insert_before(user&,const std::string&);
        void remove_in_line(user&,int);
        public:
        document(const std::vector<std::string>&);
        void process(unsigned,const std::string&,std::ostream&);
        void print_to_file(std::ostream&);
    };
}
#endif
