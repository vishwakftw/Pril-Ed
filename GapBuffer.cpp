// Function definitions for the function prototypes in GapBuffer.hpp
#include <algorithm>
#include <cassert>
#include "GapBuffer.hpp"
#include <iostream>
namespace para {

    // Constructing with an initializer list
    gap_buffer::gap_buffer(const std::string& init) : buffer([&init](){
        std::vector<char> temp;
        std::copy(std::begin(init),std::end(init),std::back_inserter(temp));
        std::generate_n(std::back_inserter(temp),min_gap_size*2,[](){return '\0';});
        return temp;
    }()), gap_start(buffer.size()-(2*min_gap_size)),gap_end(buffer.size()){
        static_assert(2*min_gap_size<=max_gap_size,"Invalid gap size constraints!");
    }

    // invariant check
    bool gap_buffer::invariant() const noexcept {
        return (gap_size()>=min_gap_size)&&(gap_size()<max_gap_size);
    }

    // Function to return the current size of the gap whenever called
    unsigned gap_buffer::gap_size() const noexcept {
        return gap_end-gap_start;
    }

    // Moving the "cursor" within the gap buffer relative to current position
    void gap_buffer::relative_move(int offset) {
        assert((offset==0)||(offset>0&&gap_end+offset<=buffer.size())||(offset<0&&gap_start+offset>=0)); //precondition
        if (offset>0) { //move cursor to right
            std::move(std::begin(buffer)+gap_end,std::begin(buffer)+gap_end+offset,std::begin(buffer)+gap_start);
        }
        else if (offset<0) { //move cursor to left
            std::move_backward(std::begin(buffer)+gap_start+offset,std::begin(buffer)+gap_start,std::begin(buffer)+gap_end);
        }
        gap_end=gap_end+offset;         // Modify the start and end appropriately
        gap_start=gap_start+offset;
    }

    // Function to move to an absolute location
    void gap_buffer::absolute_move(unsigned location) {
        if (location<gap_start) {
            auto size=gap_size();
            std::move_backward(std::begin(buffer)+location,std::begin(buffer)+gap_start,std::begin(buffer)+gap_end);
            gap_start=location;
            gap_end=gap_start+size;
        }
        else if (location>gap_start) {
            auto size=gap_size();
            std::move(std::begin(buffer)+location,std::begin(buffer)+gap_start,std::begin(buffer)+location+size);
            gap_start=location;
            gap_end=gap_start+size;
        }
    }

    // Function to alter size of the gap
    void gap_buffer::alter_gap_size(int delta) {
        assert(gap_size()+delta>=0);
        if (delta>0) {
            buffer.insert(std::begin(buffer)+gap_end,delta,'\0');
        }
        else if (delta<0) {
            buffer.erase(std::begin(buffer)+gap_end+delta,std::begin(buffer)+gap_end);
        }
        gap_end+=delta;
    }

    // Function to insert new text in the buffer
    void gap_buffer::insert(const std::string& data) {
        assert(invariant());
        if (int{gap_size()}-int{data.size()}<int{min_gap_size}) { //need to increase gap size for this operation
            alter_gap_size(int{data.size()}-(int{gap_size()}-int{min_gap_size}));
        }
        std::copy(std::begin(data),std::end(data),std::begin(buffer)+gap_start);
        gap_start+=data.size();
        assert(invariant());
    }

    // Function to remove pre-existing text in buffer
    void gap_buffer::remove(int offset) {

        //Deletion possible in both directions
        assert((offset>0&&gap_end+offset<=buffer.size())||(offset<0&&gap_start+offset>=0) || offset == 0);
        assert(invariant());
        if (offset>0) { //deleting forward
            gap_end+=offset;
        }
        else if (offset<0) { //deleting backward
            gap_start+=offset;
        }
        if (gap_size()>=max_gap_size) {
            alter_gap_size(gap_size()-min_gap_size);
        }
        assert(invariant());
    }
    unsigned gap_buffer::length() const noexcept {      // Function to return the current length of the buffer
        return buffer.size()-gap_size();
    }
    std::string gap_buffer::reconstruct() const noexcept {  // Function that returns the contents of the buffer (except the gap). So techncically it returns out the text present in the buffer.
        std::string temp;
        std::copy(std::begin(buffer),std::begin(buffer)+gap_start,std::back_inserter(temp));
        std::copy(std::begin(buffer)+gap_end,std::end(buffer),std::back_inserter(temp));
        return temp;
    }
    void gap_buffer::replace(const std::string& text) {     // Function to replace given text with another text
        assert(invariant());
        buffer.resize(text.size()+2*min_gap_size);
        std::copy(std::begin(text),std::end(text),std::begin(buffer));
        gap_start=text.size();
        gap_end=buffer.size();
        assert(invariant());
    }
}
