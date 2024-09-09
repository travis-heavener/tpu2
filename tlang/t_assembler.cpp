#include <fstream>
#include <string>
#include <vector>

#include "t_assembler.hpp"
#include "t_exception.hpp"
#include "scope.hpp"
#include "ast/ast.hpp"

#define FUNC_MAIN_LABEL "main"
#define FUNC_LABEL_PREFIX "__UF" // for "user function"
#define JMP_LABEL_PREFIX "__J" // really just used for jmp instructions
#define TAB "    "

static size_t nextJMPLabelID = 0;

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
            FUNC_MAIN_LABEL : (FUNC_LABEL_PREFIX + std::to_string(nextFunctionID++));

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
    // store the current stack pointer value in the base pointer for this scope
    outHandle << TAB << "mov BP, SP\n";

    // store the number of times the stack pointer has been pushed in the scope
    size_t numPushes = 0;

    // iterate over children of this node
    size_t numChildren = pHead->size();
    for (size_t i = 0; i < numChildren; i++) {
        ASTNode& child = *pHead->at(i);

        // switch on node type
        switch (child.getNodeType()) {
            case ASTNodeType::VAR_DECLARATION: {
                // create space on stack
                ASTVarDeclaration& varChild = *static_cast<ASTVarDeclaration*>(&child);
                const TokenType varType = varChild.getPrimitiveType();
                const size_t typeSize = getSizeOfType(varType);

                // get the value of the assignment
                if (varChild.pExpr == nullptr) { // no assignment, set to 0
                    for (size_t j = 0; j < typeSize; j++)
                        outHandle << TAB << "push 0\n";
                    numPushes += typeSize;
                } else { // has assignment, assemble its expression
                    // push to stack (lowest-first)
                    size_t resultSize = assembleExpression(*varChild.pExpr, outHandle, labelMap, scope, numPushes);

                    // verify result size does not overflow
                    if (resultSize > typeSize)
                        throw std::runtime_error("resultSize of assembled expression exceeds its specified type size!");

                    // fill remaining spots with zeros
                    for (size_t j = resultSize; j < typeSize; j++)
                        outHandle << TAB << "push 0\n";
                    numPushes += typeSize - resultSize;
                }
                
                // add variable to scope
                scope.declareVariable(varType, varChild.pIdentifier->raw, varChild.pIdentifier->err);
                break;
            }
            case ASTNodeType::EXPR: {
                // assemble expression
                size_t resultSize = assembleExpression(child, outHandle, labelMap, scope, numPushes);

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    outHandle << TAB << "pop\n";
                    numPushes--;
                }
                break;
            }
            default: {
                throw std::invalid_argument("Unimplemented ASTNodeType!");
            }
        }
    }

    /* NOTE: since the SP is reset to the BP after the scope closes, this snippet is unnecessary
        
        // pop variables off the stack
        while (numPushes > 0) {
            size_t numPopped = scope.pop();
            numPushes -= numPopped;
            for (size_t i = 0; i < numPopped; i++)
                outHandle << TAB << "pop\n";
        }
    */

    // reset the stack pointer to the base pointer
    outHandle << TAB << "mov SP, BP\n";
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
    bodyNode.isAssembled = true;
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::UNARY_OP: {
            ASTOperator& unaryOp = *static_cast<ASTOperator*>(&bodyNode);

            // move argument into AX
            if (resultSizes.size() != 1)
                throw std::runtime_error("Invalid number of resultSizes, expected 1 for unary operation.");

            // ignore OP_ADD since it's really a buffer (passthrough)
            if (unaryOp.getOpTokenType() == TokenType::OP_ADD) return resultSizes[0];

            // pop in reverse (higher first, later first)
            const size_t resultSize = resultSizes[0];
            if (resultSize == 2) outHandle << TAB << "pop AH\n";
            outHandle << TAB << "pop AL\n";
            numPushes -= resultSize;

            const std::string regA = resultSize == 1 ? "AL" : "AX";

            switch (unaryOp.getOpTokenType()) {
                case TokenType::OP_SUB: {
                    // negate AX (A ^ 0b10000000 flips sign bit)
                    outHandle << TAB << "xor " << regA << ", " << (resultSize > 1 ? "0x8000" : "0x80") << '\n';
                    
                    // push values to stack
                    outHandle << TAB << "push AL\n";
                    if (resultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += resultSize;
                    return resultSize;
                }
                case TokenType::OP_BIT_NOT: {
                    // flip all bits
                    outHandle << TAB << "not " << regA << '\n';
                    
                    // push values to stack
                    outHandle << TAB << "push AL\n";
                    if (resultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += resultSize;
                    return resultSize;
                }
                case TokenType::OP_BOOL_NOT: {
                    // set non-zero to zero, zero to 1
                    const std::string labelZero = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerge = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    outHandle << TAB << "add " << regA << ", 0\n";
                    outHandle << TAB << "jz " << labelZero << '\n'; // zero, set to 1
                    outHandle << TAB << "mov " << regA << ", 0\n"; // currently non-zero, set to 0
                    outHandle << TAB << "jmp " << labelMerge << '\n'; // reconvene branches

                    outHandle << TAB << labelZero << ":\n"; // currently zero, set to 1
                    outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelMerge << '\n'; // reconvene branches
                    outHandle << TAB << labelMerge << ":\n"; // reconvene branches

                    // push values to stack
                    outHandle << TAB << "push AL\n";
                    if (resultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += resultSize;
                    return resultSize;
                }
                default:
                    throw std::invalid_argument("Invalid unaryOp type in assembleExpression!");
            }
            break;
        }
        case ASTNodeType::BIN_OP: {
            ASTOperator& binOp = *static_cast<ASTOperator*>(&bodyNode);

            // move both arguments into AX & BX
            if (resultSizes.size() != 2)
                throw std::runtime_error("Invalid number of resultSizes, expected 2 for binary operation.");
            unsigned char maxResultSize = std::max(resultSizes[0], resultSizes[1]);

            // pop in reverse (higher first, later first)
            if (maxResultSize == 2) {
                if (resultSizes[1] == 2) // there's a value here
                    outHandle << TAB << "pop BH\n";
                else // no value, but zero the top half of the register to prevent miscalculations
                    outHandle << TAB << "xor BH, BH\n";
            }
            outHandle << TAB << "pop BL\n";

            // ignore popping to the AL/AX register if nothing was pushed from the stack (for assignments)
            if (resultSizes[0] > 0) {
                if (maxResultSize == 2) {
                    if (resultSizes[0] == 2) // there's a value here
                        outHandle << TAB << "pop AH\n";
                    else // no value, but zero the top half of the register to prevent miscalculations
                        outHandle << TAB << "xor AH, AH\n";
                }
                outHandle << TAB << "pop AL\n";
            }

            numPushes -= resultSizes[0] + resultSizes[1];

            const std::string regA = maxResultSize == 1 ? "AL" : "AX";
            const std::string regB = maxResultSize == 1 ? "BL" : "BX";

            switch (binOp.getOpTokenType()) {
                case TokenType::OP_ADD: {
                    // add AX/AL to BX/BL
                    outHandle << TAB << "add " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_SUB: {
                    // sub AX/AL to BX/BL
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_MUL: {
                    // mul by BX/BL
                    outHandle << TAB << "mul " << regB << '\n';

                    // push result to stack (lowest-first)
                    // max size is 16-bit so just ignore overflow
                    outHandle << TAB << "push AL\n" << TAB << "push AH\n";
                    numPushes += 2;
                    return 2;
                }
                case TokenType::OP_DIV: {
                    // div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_MOD: {
                    // get mod from div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "push DL\n" << TAB << "push DH\n";
                    else
                        outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_BIT_OR: {
                    outHandle << TAB << "or " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_BIT_AND: {
                    outHandle << TAB << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_BIT_XOR: {
                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_BOOL_OR: {
                    outHandle << TAB << "or " << regA << ", " << regB << '\n';

                    // if ZF is cleared, expression is true (set to 1)
                    const std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_BOOL_AND: {
                    // if either one is zero, boolean and is false

                    // put 0 into regA if zero, 1 otherwise
                    // if ZF is set, set regA (non-zero -> 1)
                    std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "or " << regA << ", 0\n"; // sets ZF if 0
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // put 0 into regB if zero, 1 otherwise
                    // if ZF is set, set regB (non-zero -> 1)
                    labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "or " << regB << ", 0\n"; // sets ZF if 0
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    outHandle << TAB << "mov " << regB << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // push regA & regB (0b0 & 0b1 or some combination)
                    outHandle << TAB << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_EQ: {
                    // if A ^ B is zero, equal
                    const std::string labelEQ = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';

                    outHandle << TAB << "jz " << labelEQ << '\n';
                    outHandle << TAB << "mov " << regA << ", 0\n"; // non-zero becomes 0
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    outHandle << TAB << labelEQ << ":\n";
                    outHandle << TAB << "mov " << regA << ", 1\n"; // zero becomes 1
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_NEQ: {
                    // if A ^ B is zero, equal (keep as 0)
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';
                    outHandle << TAB << "jz " << labelMerger << '\n';
                    outHandle << TAB << "mov " << regA << ", 1\n"; // non-zero becomes 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_LT: {
                    // if A < B, B-A will have carry and zero cleared
                    outHandle << TAB << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelGTE << '\n'; // zero is set, A == B
                    outHandle << TAB << "jc " << labelGTE << '\n'; // carry is set, A > B
                    outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelGTE << ":\n"; // set regA to 0
                    outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_GT: {
                    // if A > B, A-B will have carry and zero cleared
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelLTE << '\n'; // zero is set, A == B
                    outHandle << TAB << "jc " << labelLTE << '\n'; // carry is set, A > B
                    outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelLTE << ":\n"; // set regA to 0
                    outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_LTE: {
                    // if A <= B, B-A will have carry cleared
                    outHandle << TAB << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jc " << labelGT << '\n'; // carry is set, A > B
                    outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelGT << ":\n"; // set regA to 0
                    outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_GTE: {
                    // if A >= B, A-B will have carry cleared
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jc " << labelLT << '\n'; // carry is set, A > B
                    outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelLT << ":\n"; // set regA to 0
                    outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_LSHIFT: {
                    // can only use 8-bit register for shift count
                    outHandle << TAB << "shl " << regA << ", BL\n";

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::OP_RSHIFT: {
                    // can only use 8-bit register for shift count
                    outHandle << TAB << "shr " << regA << ", BL\n";

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    if (maxResultSize == 2) outHandle << TAB << "push AH\n";
                    numPushes += maxResultSize;
                    return maxResultSize;
                }
                case TokenType::ASSIGN: {
                    // get the stack offset of the variable
                    if (bodyNode.at(0)->getNodeType() != ASTNodeType::IDENTIFIER)
                        // throw TInvalidTokenException(bodyNode.at(0)->err);
                        throw std::runtime_error(bodyNode.at(0)->raw + "\n" + bodyNode.raw);

                    ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(bodyNode.at(0));
                    ScopeVariable* pScopeVar = scope.getVariable(identifier.raw, identifier.err);
                    size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);
                    size_t typeSize = getSizeOfType(pScopeVar->getType());

                    // factor in additional pushes/pops that aren't scope variables (ex. pushes for expressions)
                    stackOffset += numPushes - scope.sizeBytes();

                    // move the rvalue to the identifier on the left
                    outHandle << TAB << "rmov $-" << stackOffset << ", BL" << '\n';
                    if (resultSizes[1] == 2)
                        outHandle << TAB << "rmov $-" << stackOffset-1 << ", BH" << '\n';
                    
                    // push the value of the variable onto the stack (lowest-first)
                    outHandle << TAB << "push BL\n";
                    if (typeSize == 2) outHandle << TAB << "push BH\n";
                    numPushes += typeSize;
                    return typeSize;
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
            numPushes += size;
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
            numPushes += size;
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
            numPushes += size;
            return size;
        }
        case ASTNodeType::LIT_FLOAT: {
            throw std::invalid_argument("Float arithmetic not implemented yet!");
        }
        case ASTNodeType::IDENTIFIER: {
            // lookup identifier
            ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(&bodyNode);
            ScopeVariable* pScopeVar = scope.getVariable(identifier.raw, identifier.err);
            size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);
            size_t typeSize = getSizeOfType(pScopeVar->getType());

            // factor in additional pushes/pops that aren't scope variables (ex. pushes for expressions)
            stackOffset += numPushes - scope.sizeBytes();

            // handle assignment operations vs read operations
            if (!identifier.isInAssignExpr) { // read operation, so push the value of the identifier onto the stack
                for (size_t i = 0; i < typeSize; i++) {
                    // move to DL to buffer pushing value in memory to stack
                    outHandle << TAB << "rmov DL, $-" << stackOffset << '\n'; // don't do stackOffset-i because as things are pushed, this offset changes
                    outHandle << TAB << "push DL\n";
                }
                numPushes += typeSize;
                return typeSize;
            }

            // base case, assignment operation, push nothing to the stack
            return 0;
        }
        case ASTNodeType::EXPR: {
            // this is the top-most expression, so just pass through
            return resultSizes[0];
        }
        default: {
            throw std::invalid_argument("Invalid node type in assembleExpression!");
        }
    }

    // base case, NEVER reached
    throw std::invalid_argument("How did you even get here?");
    return 0;
}