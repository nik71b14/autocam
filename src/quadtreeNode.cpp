#include "QuadTreeNode.hpp"

// Constructor
QuadTreeNode::QuadTreeNode(QuadValue value, const std::array<std::shared_ptr<QuadTreeNode>, 4>& children)
    : value(value), children(children) {
    if (value == QuadValue::MIXED && !children.empty()) {
        this->children = children;
    }
}

// Helper function to get a bit from a vector of bits
bool QuadTreeNode::getBit(const std::vector<uint8_t>& bits, size_t index) {
    size_t byteIndex = index / 8;
    size_t bitIndex = index % 8;
    if (byteIndex >= bits.size()) return false; // optional safety
    return (bits[byteIndex] >> bitIndex) & 1;
}

// Static method to create a node from a bit matrix
std::shared_ptr<QuadTreeNode> QuadTreeNode::fromBitMatrix(const std::vector<uint8_t>& matrix, 
                                                          size_t size, 
                                                          size_t x, 
                                                          size_t y, 
                                                          size_t span) {
    if (span == 0) span = size;
    
    if (span == 1) {
        size_t index = y * size + x;

        //@@@ Check if the index is within bounds
        // if (index >= matrix.size()) {
        //     if (index >= matrix.size()) {
        //         std::cerr << "Error: Index " << index << " is out of bounds for matrix of size " << matrix.size() << std::endl;
        //     }
        //     throw std::out_of_range("Index out of bounds in fromBitMatrix");
        // }
        // if (matrix[index] != 0 && matrix[index] != 1) {
        //     throw std::invalid_argument("Matrix must contain only 0s and 1s");
        // }
        return std::make_shared<QuadTreeNode>(QuadTreeNode::getBit(matrix, index) ? QuadValue::FILLED : QuadValue::EMPTY);
        // return std::make_shared<QuadTreeNode>(matrix[index] ? QuadValue::FILLED : QuadValue::EMPTY);
    }

    size_t half = span / 2;
    std::array<std::shared_ptr<QuadTreeNode>, 4> children = {
        fromBitMatrix(matrix, size, x, y, half),
        fromBitMatrix(matrix, size, x + half, y, half),
        fromBitMatrix(matrix, size, x, y + half, half),
        fromBitMatrix(matrix, size, x + half, y + half, half)
    };

    bool allSame = true;
    for (size_t i = 1; i < 4; ++i) {
        if (children[i]->value != children[0]->value) {
            allSame = false;
            break;
        }
    }

    if (allSame && children[0]->value != QuadValue::MIXED) {
        return std::make_shared<QuadTreeNode>(children[0]->value); // Collapse
    }

    return std::make_shared<QuadTreeNode>(QuadValue::MIXED, children);
}

// Get the size of the tree (number of nodes)
size_t QuadTreeNode::getSize() const {
    if (children[0] == nullptr) return 1; // Leaf node
    size_t sum = 1;
    for (const auto& child : children) {
        if (child != nullptr) {
            sum += child->getSize();
        }
    }
    return sum;
}

// Carve (modify the node according to the mask)
void QuadTreeNode::carve(std::shared_ptr<QuadTreeNode> mask) {
    if (value == QuadValue::EMPTY || mask->value == QuadValue::EMPTY) return;

    if (value == QuadValue::FILLED && mask->value == QuadValue::FILLED) {
        value = QuadValue::EMPTY;
        children.fill(nullptr);  // Remove children
        return;
    }

    if (children[0] != nullptr && mask->children[0] != nullptr) {
        for (size_t i = 0; i < 4; ++i) {
            children[i]->carve(mask->children[i]);
        }

        bool allSame = true;
        for (size_t i = 1; i < 4; ++i) {
            if (children[i]->value != children[0]->value) {
                allSame = false;
                break;
            }
        }

        if (allSame && children[0]->value != QuadValue::MIXED) {
            value = children[0]->value;
            children.fill(nullptr);  // Remove children
        }
    }
}
