#include <fstream>
#include <string>
#include <vector>

#include "t_assembler.hpp"
#include "scope.hpp"
#include "ast/ast.hpp"

#define FUNC_MAIN_LABEL "main"
#define FUNC_LABEL_PREFIX "__UF" // for "user function"
#define TAB "    "

// generate TPU assembly code from the AST
void generateAssembly(AST& ast, std::ofstream& outHandle) {
    // initialize list of function labels
    size_t nextFunctionID = 0;
    label_map_t labelMap; // function name, function label

    // iterate over global functions
    std::vector<ASTNode*>& globalChildren = ast.getChildren();
    for (ASTNode* pFunc : globalChildren) {
        // build list of function labels
        ASTFunction& funcNode = *static_cast<ASTFunction*>(pFunc);
        const std::string funcName = funcNode.getName();
        const std::string labelName = funcName == FUNC_MAIN_LABEL ?
            FUNC_MAIN_LABEL : FUNC_LABEL_PREFIX + nextFunctionID++;

        // record function name
        labelMap[funcName] = labelName;

        // assemble this function into the file
        outHandle << labelName << ":\n";

        // create a scope for this body
        Scope scope;

        // assemble body content
        assembleBody(pFunc, outHandle, labelMap, scope);

        // stop clock after execution is done
        outHandle << TAB << "hlt\n";
    }
}

// for assembling body content that has its own scope
void assembleBody(ASTNode* pHead, std::ofstream& outHandle, label_map_t& labelMap, Scope& scope) {
    // store the number of times the stack pointer has been pushed in the scope
    size_t numPushes = 0;

    // iterate over children of this node
    size_t numChildren = pHead->size();
    for (size_t i = 0; i < numChildren; i++) {
        ASTNode& child = *pHead->at(i);

        // switch on node type
        switch (child.getNodeType()) {
            case ASTNodeType::EXPR: {
                // assemble expression
                size_t resultSize = assembleExpression(child, outHandle, labelMap, scope, numPushes);
                break;
            }
        }
    }

    // revert the stack back for each pushed scope
    while (numPushes > 0)
        numPushes -= scope.pop(); // pops last variable off stack's scope, returns # of bytes freed
}

// assembles an expression, returning the number of bytes the result uses on the stack
size_t assembleExpression(ASTNode& bodyNode, std::ofstream& outHandle, label_map_t& labelMap, Scope& scope, size_t& numPushes) {
    // recurse this function for children, bottom-up
    size_t numChildren = bodyNode.size();
    std::vector<size_t> resultSizes;
    for (size_t i = 0; i < numChildren; ++i) {
        resultSizes.push_back(
            assembleExpression(*bodyNode.at(i), outHandle, labelMap, scope, numPushes)
        );
    }

    // assemble this node
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::BIN_OP: {
            ASTOperator& binOp = *static_cast<ASTOperator*>(&bodyNode);

            // move both arguments into AX & BX
            unsigned char maxResultSize = std::max(resultSizes[0], resultSizes[1]);

            // pop in reverse (higher first, later first)
            if (resultSizes[1] == 2) outHandle << TAB << "pop BH\n";
            outHandle << TAB << "pop BL\n";

            if (resultSizes[0] == 2) outHandle << TAB << "pop AH\n";
            outHandle << TAB << "pop AL\n";

            const std::string regA = maxResultSize == 1 ? "AL" : "AX";
            const std::string regB = maxResultSize == 1 ? "BL" : "BX";

            switch (binOp.getOpTokenType()) {
                case TokenType::OP_ADD: {
                    // add AX/AL to BX/BL
                    outHandle << TAB << "add " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_SUB: {
                    // sub AX/AL to BX/BL
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_MUL: {
                    // mul by BX/BL
                    outHandle << TAB << "mul " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n" << TAB << "push AH\n";
                    if (maxResultSize > 1) // uses AX as lower and DX as upper
                        outHandle << TAB << "push DL\n" << TAB << "push DH\n";
                    return maxResultSize;
                }
                case TokenType::OP_DIV: {
                    // div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_MOD: {
                    // get mod from div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize > 1) // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "push DL\n" << TAB << "push DH\n";
                    else
                        outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_BIT_OR: {
                    outHandle << TAB << "or " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_BIT_AND: {
                    outHandle << TAB << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                case TokenType::OP_BIT_XOR: {
                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize > 1) outHandle << TAB << "push AH\n";
                    return maxResultSize;
                }
                default:
                    throw std::invalid_argument("Invalid binOp type in assembleExpression!");
            }
            break;
        }
        case ASTNodeType::LIT_INT: {
            // get value & size of literal
            int value = static_cast<ASTIntLiteral*>(&bodyNode)->val;
            size_t size = getSizeOfType(TokenType::TYPE_INT);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                outHandle << TAB << "push " << (value & 0xFF) << '\n';
                value >>= 8; // shift downward
            }
            return size;
        }
        case ASTNodeType::LIT_BOOL: {
            // get value & size of literal
            bool value = static_cast<ASTBoolLiteral*>(&bodyNode)->val;
            size_t size = getSizeOfType(TokenType::TYPE_BOOL);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                outHandle << TAB << "push " << (value & 0xFF) << '\n';
                value >>= 8; // shift downward
            }
            return size;
        }
        case ASTNodeType::LIT_CHAR: {
            // get value & size of literal
            char value = static_cast<ASTCharLiteral*>(&bodyNode)->val;
            size_t size = getSizeOfType(TokenType::TYPE_CHAR);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                outHandle << TAB << "push " << (value & 0xFF) << '\n';
                value >>= 8; // shift downward
            }
            return size;
        }
        case ASTNodeType::LIT_FLOAT: {
            throw std::invalid_argument("Float arithmetic not implemented yet!");
        }
        case ASTNodeType::EXPR: {
            // this is the top-most expression, so just pass through
            return resultSizes[0];
        }
        default: {
            throw std::invalid_argument("Invalid node type in assembleExpression!");
        }
    }
}