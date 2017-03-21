#ifndef PARA_GAP_BUFFER_HPP
#define PARA_GAP_BUFFER_HPP
#include <vector>
#include <string>
namespace para {
    class gap_buffer {
        using buffer_t=std::vector<char>;
        buffer_t buffer;
        using elem_ref=unsigned;
        elem_ref gap_start;
        elem_ref gap_end; //cursor is always at gap_start
        constexpr static const unsigned min_gap_size=8u;
        constexpr static const unsigned max_gap_size=64u;
        bool invariant() const noexcept;
        unsigned gap_size() const noexcept;
        void alter_gap_size(int); //change gap length by the delta
        public:
        gap_buffer(const std::string&);
        void relative_move(int); //moves cursor by offset values(can be + or -)
        void absolute_move(unsigned); //moves cursor to absolute location(it moves to before location)
        void insert(const std::string&); //inserts data at cursor
        void remove(int); //removes data from current location to offset
        std::string reconstruct() const noexcept;
        void replace(const std::string&);
        unsigned length() const noexcept;
    };
}
#endif
