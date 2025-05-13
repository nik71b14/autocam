#ifndef QUAD_TREE_NODE_HPP
#define QUAD_TREE_NODE_HPP

#include <iostream>
#include <array>
#include <memory>
#include <vector>

enum class QuadValue {
    EMPTY = 0,
    FILLED = 1,
    MIXED = 2
};

class QuadTreeNode {
public:
    QuadValue value;
    std::array<std::shared_ptr<QuadTreeNode>, 4> children;

    // Constructor
    QuadTreeNode(QuadValue value = QuadValue::EMPTY, 
                 const std::array<std::shared_ptr<QuadTreeNode>, 4>& children = {});

    // Helper method
    static bool getBit(const std::vector<uint8_t>& bits, size_t index);

    // Static method to create a node from a bit matrix
    static std::shared_ptr<QuadTreeNode> fromBitMatrix(const std::vector<uint8_t>& matrix, 
                                                       size_t size, 
                                                       size_t x = 0, 
                                                       size_t y = 0, 
                                                       size_t span = 0);

    // Get the size of the tree (number of nodes)
    size_t getSize() const;

    // Carve (modify the node according to the mask)
    void carve(std::shared_ptr<QuadTreeNode> mask);
};

#endif // QUAD_TREE_NODE_HPP
