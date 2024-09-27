#include <fstream>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "ast/ast.hpp"
#include "util/toolbox.hpp"
#include "util/token.hpp"
#include "util/type.hpp"
#include "util/t_exception.hpp"
#include "util/err_info.hpp"

#define FUNC_MAIN_LABEL "main"
#define FUNC_LABEL_PREFIX "__UF" // for "user function"
#define FUNC_END_LABEL_SUFFIX "E" // added to the end of a function label to mark where a function ends
#define JMP_LABEL_PREFIX "__J" // really just used for jmp instructions
#define TAB "    "

// staic fields
static size_t nextFuncLabelID = 0;
static size_t nextJMPLabelID = 0;
static label_map_t labelMap;

// generate TPU assembly code from the AST
void generateAssembly(AST& ast, std::ofstream& outHandle) {
    // iterate over global functions
    std::vector<ASTNode*>& globalChildren = ast.getChildren();
    for (ASTNode* pFunc : globalChildren) {
        // build list of function labels
        ASTFunction& funcNode = *static_cast<ASTFunction*>(pFunc);

        // assemble function
        assembleFunction(funcNode, outHandle);
    }
}

// for assembling functions
void assembleFunction(ASTFunction& funcNode, std::ofstream& outHandle) {
    // determine labelName
    const std::string funcName = funcNode.getName();
    const std::string labelName = funcName == FUNC_MAIN_LABEL ?
        FUNC_MAIN_LABEL : (FUNC_LABEL_PREFIX + std::to_string(nextFuncLabelID++));

    // record function name & return type
    labelMap[funcName] = {labelName, labelName + FUNC_END_LABEL_SUFFIX, funcNode.getReturnType()};

    // create a scope for this body
    Scope scope;

    // assemble this function into the file
    outHandle << labelName << ":\n";

    // if this is main, add return byte spacing on top of stack
    const size_t returnSize = funcNode.getReturnType().getSizeBytes();
    if (funcName == FUNC_MAIN_LABEL && returnSize > 0)
        OUT << "add SP, " << returnSize << '\n';

    // add function args to scope (args on top of stack below return bytes)
    for (size_t i = 0; i < funcNode.getNumParams(); i++) { // add the variable to the scope
        ASTFuncParam* pArg = funcNode.paramAt(i);
        scope.declareFunctionParam(pArg->type, pArg->name, funcNode.err);
    }

    // add return bytes to scope
    if (funcNode.getReturnType().getSizeBytes() > 0)
        scope.declareVariable(funcNode.getReturnType(), SCOPE_RETURN_START, funcNode.err);

    // assemble body content
    bool hasReturned = assembleBody(&funcNode, outHandle, scope, funcName, true, true);

    // declare end of function label
    if (!hasReturned && returnSize > 0)
        throw std::invalid_argument("Missing return statement in non-void function: " + funcName);
    OUT << "jmp " << labelName + FUNC_END_LABEL_SUFFIX << '\n';

    // write return label
    OUT << labelName + FUNC_END_LABEL_SUFFIX << ":\n";

    // stop clock after execution is done IF MAIN or return to previous label
    if (funcName == FUNC_MAIN_LABEL) {
        // handle return status
        OUT << "movw AX, 0x03\n"; // specify syscall type
        OUT << "popw BX\n"; // pop return status to AX
        OUT << "syscall\n"; // trigger syscall
        OUT << "hlt\n";
    } else { // return to previous label from call instruction
        OUT << "ret\n";
    }
}

// for assembling body content that may or may not have its own scope
// returns true if the current body has returned (really only matters in function scopes)
bool assembleBody(ASTNode* pHead, std::ofstream& outHandle, Scope& scope, const std::string& funcName, const bool isNewScope, const bool isTopScope) {
    size_t startingScopeSize = scope.size(); // remove any scoped variables at the end
    const std::string returnLabel = labelMap[funcName].returnLabel;
    const Type desiredType = labelMap[funcName].returnType;
    const size_t returnSize = desiredType.getSizeBytes();

    // iterate over children of this node
    bool hasReturned = false;
    size_t numChildren = pHead->size();
    for (size_t i = 0; i < numChildren && !hasReturned; i++) {
        ASTNode& child = *pHead->at(i);

        // switch on node type
        switch (child.getNodeType()) {
            case ASTNodeType::WHILE_LOOP: {
                ASTWhileLoop& loop = *static_cast<ASTWhileLoop*>(&child);

                // create label for the start of the loop body (here)
                const std::string loopStartLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // determine the label where all branches merge
                const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // append loopStartLabel
                OUT << loopStartLabel << ":\n";

                // check condition
                size_t resultSize = assembleExpression(*loop.pExpr, outHandle, scope).getSizeBytes();

                // load result to AL/AX
                if (resultSize == 2) {
                    OUT << "pop AX\n"; // load value to AH
                    scope.pop();
                } else {
                    OUT << "xor AH, AH\n"; // clear AH if no value is there
                    OUT << "pop AL\n";
                }
                scope.pop();
                OUT << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                OUT << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                bool hasReturnedLocal = assembleBody(&loop, outHandle, scope, funcName);
                hasReturned = !isTopScope && hasReturnedLocal;

                // jump back to the loopStartLabel
                OUT << "jmp " << loopStartLabel << '\n';

                // add merge label
                OUT << mergeLabel << ":\n";
                break;
            }
            case ASTNodeType::FOR_LOOP: {
                ASTForLoop& loop = *static_cast<ASTForLoop*>(&child);

                // assemble first expression in current scope
                size_t resultSize = assembleExpression(*loop.pExprA, outHandle, scope).getSizeBytes();

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    if (j+1 < resultSize) {
                        OUT << "popw\n";
                        j++;
                        scope.pop(); // additional pop
                    } else {
                        OUT << "pop\n";
                    }
                    scope.pop();
                }

                // create label for the start of the loop body (here)
                const std::string loopStartLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // determine the label where all branches merge
                const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // append loopStartLabel
                OUT << loopStartLabel << ":\n";

                // check condition
                resultSize = assembleExpression(*loop.pExprB, outHandle, scope).getSizeBytes();

                // load result to AL/AX
                if (resultSize == 2) {
                    OUT << "popw AX\n"; // load value to AH
                    scope.pop();
                } else {
                    OUT << "xor AH, AH\n"; // clear AH if no value is there
                    OUT << "pop AL\n";
                }
                scope.pop();
                OUT << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                OUT << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                bool hasReturnedLocal = assembleBody(&loop, outHandle, scope, funcName);
                hasReturned = !isTopScope && hasReturnedLocal;

                // assemble third expression
                resultSize = assembleExpression(*loop.pExprC, outHandle, scope).getSizeBytes();

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    if (j+1 < resultSize) {
                        OUT << "popw\n";
                        j++;
                        scope.pop(); // additional pop
                    } else {
                        OUT << "pop\n";
                    }
                    scope.pop();
                }

                // jump back to the loopStartLabel
                OUT << "jmp " << loopStartLabel << '\n';

                // add merge label
                OUT << mergeLabel << ":\n";
                break;
            }
            case ASTNodeType::CONDITIONAL: {
                ASTConditional& conditional = *static_cast<ASTConditional*>(&child);

                // determine the label where all branches merge
                const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // for all children, parse the expression
                for (size_t j = 0; j < conditional.size(); j++) {
                    const std::string nextLabel = j+1 == conditional.size() ?
                        mergeLabel : JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    
                    ASTNodeType branchType = conditional.at(j)->getNodeType();
                    if (branchType != ASTNodeType::ELSE_CONDITION) {
                        // parse the expression
                        ASTNode* pExpr;
                        if (branchType == ASTNodeType::IF_CONDITION) // if branch
                            pExpr = static_cast<ASTIfCondition*>(conditional.at(j))->pExpr;
                        else // else if branch
                            pExpr = static_cast<ASTElseIfCondition*>(conditional.at(j))->pExpr;

                        assembleExpression(*pExpr, outHandle, scope);

                        // pop the result off the stack to AL
                        OUT << "xor AH, AH\n"; // clear AH since no value is put there
                        OUT << "pop AL\n"; // pop to AL
                        scope.pop();

                        // if false, jump to next condition
                        OUT << "buf AX\n"; // set ZF if false
                        OUT << "jz " << nextLabel << '\n';
                    }

                    // this is executed when not jumping anywhere (else branch or false condition)
                    // process the body in a new scope
                    bool hasReturnedLocal = assembleBody(conditional.at(j), outHandle, scope, funcName);
                    hasReturned = !isTopScope && hasReturnedLocal;

                    // jump to merge label
                    OUT << "jmp " << mergeLabel << '\n';

                    // write next label
                    OUT << nextLabel << ":\n";
                }

                break;
            }
            case ASTNodeType::VAR_DECLARATION: {
                // create space on stack
                ASTVarDeclaration* pVarChild = static_cast<ASTVarDeclaration*>(&child);
                const Type varType = pVarChild->getType();
                const size_t typeSize = varType.getSizeBytes();

                // get the value of the assignment
                if (pVarChild->pExpr == nullptr) { // no assignment, set to 0
                    for (size_t j = 0; j < typeSize; j++) {
                        if (j+1 < typeSize) {
                            OUT << "pushw 0\n";
                            ++j;
                        } else {
                            OUT << "push 0\n";
                        }
                    }
                } else { // has assignment, assemble its expression
                    assembleExpression(*pVarChild->pExpr, outHandle, scope);

                    // remove any placeholders
                    for (size_t j = 0; j < typeSize; j++) scope.pop();
                }

                // add variable to scope
                scope.declareVariable(varType, pVarChild->pIdentifier->raw, pVarChild->pIdentifier->err);
                break;
            }
            case ASTNodeType::RETURN: {
                // assemble expression (first and only child of retNode is an ASTExpr*)
                ASTReturn& retNode = *static_cast<ASTReturn*>(&child);
                Type resultType = assembleExpression(*retNode.at(0), outHandle, scope);

                // implicit cast result to desired size, if needed
                if (resultType != desiredType)
                    implicitCast(outHandle, resultType, desiredType, scope, retNode.err);

                // move result bytes to their place earlier on the stack
                for (size_t j = 0; j < returnSize; ++j) {
                    // pop top of stack into DL
                    OUT << "pop DL\n";
                    scope.pop();

                    // mov DL to return bytes location
                    size_t index = scope.getOffset(SCOPE_RETURN_START, retNode.err) - (returnSize - 1 - j);
                    OUT << "mov [SP-" << index << "], DL\n";
                }

                // jmp to the end label to finish closing the function
                hasReturned = true;
                break;
            }
            case ASTNodeType::EXPR: {
                // assemble expression
                Type resultType = assembleExpression(child, outHandle, scope);
                const size_t resultSize = resultType.getSizeBytes();

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; ++j) {
                    if (j+1 < resultSize) {
                        OUT << "popw\n";
                        scope.pop(); // additional pop
                        ++j;
                    } else {
                        OUT << "pop\n";
                    }
                    scope.pop();
                }
                break;
            }
            default: {
                throw std::invalid_argument("Unimplemented ASTNodeType!");
            }
        }
    }

    // remove extra scope variables after the scope closes
    if (isNewScope || hasReturned) {
        size_t sizeFreed = 0;
        while (scope.size() > startingScopeSize)
            sizeFreed += scope.pop();

        // move back SP
        for (size_t i = 0; i < sizeFreed; ++i) {
            if (i+1 < sizeFreed) {
                OUT << "popw\n";
                ++i;
            } else {
                OUT << "pop\n";
            }
        }
    }
    
    return hasReturned;
}

// assembles an expression, returning the type of the value pushed to the stack
Type assembleExpression(ASTNode& bodyNode, std::ofstream& outHandle, Scope& scope) {
    // if this is a literal array without a type (ie. not part of an assignment), yell at the user (LOUDLY)
    // literal arrays are ONLY allowed during assignment
    if (bodyNode.getNodeType() == ASTNodeType::LIT_ARR) {
        ASTArrayLiteral* pArr = static_cast<ASTArrayLiteral*>(&bodyNode);
        if (pArr->getType().getPrimitiveType() == TokenType::VOID) // unset
            throw TSyntaxException(bodyNode.err);
    }

    if (bodyNode.getNodeType() == ASTNodeType::ARR_SUBSCRIPT) {
        OUT << ";;;;;;;;\n";
    }

    // get desired expression type
    const Type desiredType = static_cast<ASTTypedNode*>(&bodyNode)->getType();

    // recurse this expression's children, bottom-up
    size_t numChildren = bodyNode.size();
    std::vector<Type> resultTypes;
    for (size_t i = 0; i < numChildren; ++i) {
        // prevent parsing typecasts
        if (bodyNode.at(i)->getNodeType() == ASTNodeType::TYPE_CAST) continue;

        resultTypes.push_back( assembleExpression(*bodyNode.at(i), outHandle, scope) );
    }

    // assemble this node
    Type resultType;
    bodyNode.isAssembled = true;
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::UNARY_OP: {
            ASTOperator& unaryOp = *static_cast<ASTOperator*>(&bodyNode);

            // move argument into AX
            if (resultTypes.size() != 1)
                throw std::runtime_error("Invalid number of resultTypes, expected 1 for unary operation.");

            // ignore OP_ADD since it's really a buffer (passthrough)
            if (unaryOp.getOpTokenType() == TokenType::OP_ADD) {
                resultType = resultTypes[0];
                break;
            }

            // pop in reverse (higher first, later first)
            const size_t resultSize = resultTypes[0].getSizeBytes();
            if (resultSize == 2) {
                OUT << "popw AX\n";
                scope.pop(); // additional pop
            } else if (resultSize == 1) {
                OUT << "xor AH, AH\n"; // zero out AH
                OUT << "pop AL\n";
            } else {
                throw TInvalidOperationException(bodyNode.err);
            }
            scope.pop();

            // determine output register
            const std::string regA = resultSize == 1 ? "AL" : "AX";
            const TokenType opType = unaryOp.getOpTokenType();

            switch (opType) {
                case TokenType::OP_SUB:
                case TokenType::OP_BIT_NOT: {
                    if (opType == TokenType::OP_SUB) // negate AX (A ^ 0b10000000 flips sign bit)
                        OUT << "xor " << regA << ", " << (resultSize > 1 ? "0x8000" : "0x80") << '\n';
                    else // flip all bits
                        OUT << "not " << regA << '\n';

                    // push values to stack
                    OUT << (resultSize == 2 ? "pushw AX\n" : "push AL\n");
                    scope.addPlaceholder(resultSize);
                    resultType = resultTypes[0];
                    break;
                }
                case TokenType::OP_BOOL_NOT: {
                    // set non-zero to zero, zero to 1
                    const std::string labelZero = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerge = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    OUT << "add " << regA << ", 0\n";
                    OUT << "jz " << labelZero << '\n'; // zero, set to 1
                    OUT << (regA[1] == 'X' ? "movw " : "mov ") << regA << ", 0\n"; // currently non-zero, set to 0
                    OUT << "jmp " << labelMerge << '\n'; // reconvene branches

                    OUT << labelZero << ":\n"; // currently zero, set to 1
                    OUT << (regA[1] == 'X' ? "movw " : "mov ") << regA << ", 1\n";
                    OUT << "jmp " << labelMerge << '\n'; // reconvene branches
                    OUT << labelMerge << ":\n"; // reconvene branches

                    // push values to stack
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::ASTERISK: {
                    OUT << "movw DX, BP\n"; // backup BP to DX
                    OUT << "movw BP, AX\n"; // dereference ptr whose address is stored in AX

                    // pass final result size
                    resultType = resultTypes[0];
                    resultType.popPointer();

                    // move that value to the stack
                    for (size_t k = 0; k < resultType.getSizeBytes(); ++k) {
                        OUT << "push [BP+" << k << "]\n";
                        scope.addPlaceholder();
                    }

                    OUT << "movw BP, DX\n"; // restore BP from DX
                    break;
                }
                case TokenType::AMPERSAND: {
                    // buffer result from lvalue
                    OUT << "pushw AX\n";
                    scope.addPlaceholder(2);
                    resultType = MEM_ADDR_TYPE;
                    break;
                }
                case TokenType::SIZEOF: {
                    // push the size of whatever the result on the stack is (as uint)
                    OUT << "pushw " << resultSize << '\n';
                    scope.addPlaceholder(2);
                    resultType = MEM_ADDR_TYPE;
                    break;
                }
                default: {
                    // handle typecast unary
                    if (unaryOp.getUnaryType() == ASTUnaryType::TYPE_CAST) {
                        // just use the value from the top of the stack (operand)
                        OUT << (resultSize == 2 ? "pushw AX\n" : "push AL\n");
                        scope.addPlaceholder(resultSize);
                        resultType = resultTypes[0];
                        break;
                    }
                    throw std::invalid_argument("Invalid unaryOp type in assembleExpression!");
                }
            }
            break;
        }
        case ASTNodeType::BIN_OP: {
            ASTOperator& binOp = *static_cast<ASTOperator*>(&bodyNode);

            // move both arguments into AX & BX
            if (resultTypes.size() != 2)
                throw std::runtime_error("Invalid number of resultSizes, expected 2 for binary operation.");

            // get dominant type
            const Type dominantType = getDominantType(resultTypes[0], resultTypes[1]);
            const unsigned char dominantSize = dominantType.getSizeBytes();

            // pop in reverse (higher first, later first)
            if (dominantSize < 1 || dominantSize > 2) throw TSyntaxException(bodyNode.err);

            if (resultTypes[1].getSizeBytes() == 2) { // pop to BX
                OUT << "popw BX\n";
                scope.pop();
            } else { // pop to BL and zero BH
                OUT << "xor BH, BH\n";
                OUT << "pop BL\n";
            }
            scope.pop();

            // ignore popping to the AL/AX register if nothing was pushed from the stack (for assignments)
            if (resultTypes[0].getSizeBytes() == 2) { // pop to AX
                OUT << "popw AX\n";
                scope.pop(); scope.pop();
            } else if (resultTypes[0].getSizeBytes() == 1) { // pop to AL and zero AH
                OUT << "xor AH, AH\n";
                OUT << "pop AL\n";
                scope.pop();
            }

            // determine output registers
            const std::string regA = dominantSize == 1 ? "AL" : "AX";
            const std::string regB = dominantSize == 1 ? "BL" : "BX";

            const TokenType opType = binOp.getOpTokenType();
            switch (opType) {
                case TokenType::OP_ADD: case TokenType::OP_SUB:
                case TokenType::ASTERISK: case TokenType::OP_DIV: case TokenType::OP_MOD:
                case TokenType::OP_BIT_OR: case TokenType::AMPERSAND: case TokenType::OP_BIT_XOR: {
                    // all use similar/the same process, so combined them here
                    switch (opType) {
                        case TokenType::OP_ADD:     OUT_BIN_OP_2(add); break; // add AX/AL to BX/BL
                        case TokenType::OP_SUB:     OUT_BIN_OP_2(sub); break; // sub AX/AL from BX/BL
                        case TokenType::ASTERISK:   OUT_BIN_OP_1B(mul); break; // mul AX/AL by BX/BL
                        case TokenType::OP_DIV:     OUT_BIN_OP_1B(div); break; // div AX/AL by BX/BL
                        case TokenType::OP_MOD:     OUT_BIN_OP_1B(div); break; // mod AX/AL by BX/BL
                        case TokenType::OP_BIT_OR:  OUT_BIN_OP_2(or); break; // bitwise or
                        case TokenType::AMPERSAND:  OUT_BIN_OP_2(and); break; // bitwise and
                        case TokenType::OP_BIT_XOR: OUT_BIN_OP_2(xor); break; // bitwise xor
                        default: break; // doesn't get here
                    }

                    // push result to stack (lowest-first)
                    if (opType == TokenType::OP_MOD) // uses AX as result and DX as remainder in 16-bit mode
                        OUT << (dominantSize == 2 ? "pushw DX\n" : "push AH\n");
                    else
                        OUT << (dominantSize == 2 ? "pushw AX\n" : "push AL\n");

                    // record push
                    scope.addPlaceholder(dominantSize);
                    resultType = dominantType;
                    break;
                }
                case TokenType::OP_BOOL_OR: {
                    OUT_BIN_OP_2(or);

                    // if ZF is cleared, expression is true (set to 1)
                    const std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n";
                    else                OUT << "mov " << regA << ", 1\n";
                    OUT << "jmp " << labelName << '\n'; // reconvene with other branch
                    OUT << labelName << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_BOOL_AND: { // if either one is zero, boolean and is false
                    // put 0 into regA if zero, 1 otherwise
                    // if ZF is set, set regA (non-zero -> 1)
                    std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "or " << regA << ", 0\n"; // sets ZF if 0
                    OUT << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n";
                    else                OUT << "mov " << regA << ", 1\n";
                    OUT << "jmp " << labelName << '\n'; // reconvene with other branch
                    OUT << labelName << ":\n"; // reconvene with other branch

                    // put 0 into regB if zero, 1 otherwise
                    // if ZF is set, set regB (non-zero -> 1)
                    labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "or " << regB << ", 0\n"; // sets ZF if 0
                    OUT << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regB[1] == 'X')
                        OUT << "movw " << regB << ", 1\n";
                    else
                        OUT << "mov " << regB << ", 1\n";
                    OUT << "jmp " << labelName << '\n'; // reconvene with other branch
                    OUT << labelName << ":\n"; // reconvene with other branch

                    // push regA & regB (0b0 & 0b1 or some combination)
                    OUT << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_EQ: {
                    // if A ^ B is zero, equal
                    const std::string labelEQ = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "xor " << regA << ", " << regB << '\n';

                    OUT << "jz " << labelEQ << '\n';
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n"; // non-zero becomes 0
                    else                OUT << "mov " << regA << ", 0\n"; // non-zero becomes 0
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    OUT << labelEQ << ":\n";
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // zero becomes 1
                    else                OUT << "mov " << regA << ", 1\n"; // zero becomes 1
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_NEQ: {
                    // if A ^ B is zero, equal (keep as 0)
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    OUT << "xor " << regA << ", " << regB << '\n';
                    OUT << "jz " << labelMerger << '\n';
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // non-zero becomes 1
                    else                OUT << "mov " << regA << ", 1\n"; // non-zero becomes 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_LT: {
                    // if A < B, B-A will have carry and zero cleared
                    OUT << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jz " << labelGTE << '\n'; // zero is set, A == B
                    OUT << "jc " << labelGTE << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // set regA to 1
                    else                OUT << "mov " << regA << ", 1\n"; // set regA to 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    OUT << labelGTE << ":\n"; // set regA to 0
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n";
                    else                OUT << "mov " << regA << ", 0\n";
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    /// push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_GT: {
                    // if A > B, A-B will have carry and zero cleared
                    OUT << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jz " << labelLTE << '\n'; // zero is set, A == B
                    OUT << "jc " << labelLTE << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // set regA to 1
                    else                OUT << "mov " << regA << ", 1\n"; // set regA to 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelLTE << ":\n"; // set regA to 0
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n";
                    else                OUT << "mov " << regA << ", 0\n";
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_LTE: {
                    // if A <= B, B-A will have carry cleared
                    OUT << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jc " << labelGT << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // set regA to 1
                    else                OUT << "mov " << regA << ", 1\n"; // set regA to 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelGT << ":\n"; // set regA to 0
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n"; // set regA to 1
                    else                OUT << "mov " << regA << ", 0\n"; // set regA to 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_GTE: {
                    // if A >= B, A-B will have carry cleared
                    OUT << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jc " << labelLT << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n"; // set regA to 1
                    else                OUT << "mov " << regA << ", 1\n"; // set regA to 1
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelLT << ":\n"; // set regA to 0
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n";
                    else                OUT << "mov " << regA << ", 0\n";
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_LSHIFT:
                case TokenType::OP_RSHIFT: {
                    // can only use 8-bit register for shift count
                    OUT << (opType == TokenType::OP_LSHIFT ? "shl " : "shr ") << regA << ", BL\n";

                    // push result to stack (lowest-first)
                    OUT << (dominantSize == 2 ? "pushw AX\n" : "push AL\n");
                    scope.addPlaceholder(dominantSize);
                    resultType = dominantType;
                    break;
                }
                case TokenType::ASSIGN: {
                    // take the address of the given lvalue from the stack and assign a value to it
                    OUT << "movw BP, AX\n"; // move address to BP

                    // move the rvalue to the lvalue's address
                    const size_t desiredSize = desiredType.getSizeBytes();
                    if (desiredSize == 2) {
                        OUT << "mov [BP+0], BL" << '\n';
                        OUT << "mov [BP+1], BH" << '\n';
                    } else {
                        OUT << "mov [BP-0], BL" << '\n';
                    }

                    // push the value of the variable onto the stack (lowest-first)
                    OUT << (desiredSize == 2 ? "pushw BX\n" : "push BL\n");
                    scope.addPlaceholder(desiredSize);
                    resultType = desiredType;
                    break;
                }
                default:
                    throw std::invalid_argument("Invalid binOp type in assembleExpression!");
            }
            break;
        }
        case ASTNodeType::LIT_INT: {
            // get value & size of literal
            int value = static_cast<ASTIntLiteral*>(&bodyNode)->val;
            resultType = Type(TokenType::TYPE_INT);
            OUT << "pushw " << (value & 0xFFFF) << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_BOOL: {
            // get value & size of literal
            bool value = static_cast<ASTBoolLiteral*>(&bodyNode)->val;
            resultType = Type(TokenType::TYPE_BOOL);
            OUT << "push " << (value & 0xFF) << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_CHAR: {
            // get value & size of literal
            char value = static_cast<ASTCharLiteral*>(&bodyNode)->val;
            resultType = Type(TokenType::TYPE_CHAR);
            OUT << "push " << (value & 0xFF) << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_FLOAT: {
            throw std::invalid_argument("Float arithmetic not implemented yet!");
        }
        case ASTNodeType::IDENTIFIER: {
            // lookup identifier
            ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(&bodyNode);
            size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);
            Type idenType = scope.getVariable(identifier.raw, identifier.err)->type;

            const size_t numPointers = idenType.getPointers().size();
            const size_t numSubscripts = identifier.getSubscripts().size();

            if (numSubscripts > numPointers)
                throw TInvalidOperationException(identifier.err);

            // factor in the stack offset of the variable's start
            OUT << "movw CX, SP\n"; // move SP into CX for manipulation
            OUT << "sub CX, " << stackOffset << '\n';

            // temp store the identifier's address on stack for arith manipulation
            OUT << "pushw CX\n";
            scope.addPlaceholder(2);

            // apply any subscripts
            auto subItr = identifier.getSubscripts().cbegin();
            const auto subItrEnd = identifier.getSubscripts().cend();
            for ((void)subItr; subItr != subItrEnd; ++subItr) {
                ASTArraySubscript* pSubscript = *subItr;

                // if this pointer is blank, push the pointer's pointed address
                if (*idenType.getPointers().rbegin() == TYPE_EMPTY_PTR) {
                    OUT << "popw BP\n";
                    OUT << "push [BP+0]\n";
                    OUT << "push [BP+1]\n";
                }

                // determine the size of the rest of the object
                idenType.popPointer();
                size_t chunkSize = idenType.getSizeBytes();

                // assemble subscript (ast_nodes.cpp makes sure these are all implicitly converted to int)
                assembleExpression(*pSubscript, outHandle, scope);

                OUT << "popw AX\n"; // pop subscript off stack
                scope.pop(); scope.pop();

                OUT << "popw CX\n"; // pop address back into CX
                scope.pop(); scope.pop();

                OUT << "movw BX, " << chunkSize << '\n'; // move chunkSize into BX to force 16-bit
                OUT << "mul BX\n"; // scale by chunk size

                OUT << "add CX, AX\n"; // add the chunk to the pointer

                OUT << "pushw CX\n"; // put address back onto stack
                scope.addPlaceholder(2);
            }

            // if lvalue, do nothing--address is on top of stack
            // if there are array pointers left, push the array's address (already on stack)
            if (!identifier.isLValue() && idenType.getNumArrayHints() == 0) { // otherwise, push value
                // pop address from stack to BP
                OUT << "popw BP\n";
                scope.pop(); scope.pop();

                // push bytes (lowest-first)
                for (size_t j = 0; j < idenType.getSizeBytes(); ++j) {
                    OUT << "push [BP+" << j << "]\n";
                    scope.addPlaceholder();
                }
            }

            // update result type
            resultType = idenType;
            break;
        }
        case ASTNodeType::FUNCTION_CALL: {
            // lookup the function
            ASTFunctionCall& func = *static_cast<ASTFunctionCall*>(&bodyNode);
            const std::string funcName = func.raw;
            if (labelMap.count(funcName) == 0) throw TUnknownIdentifierException(func.err);

            // allocate space on the stack for return bytes
            assembled_func_t destFunc = labelMap[funcName];
            size_t returnSize = destFunc.returnType.getSizeBytes();

            for (size_t i = 0; i < returnSize; i++) {
                if (i+1 < returnSize) {
                    OUT << "pushw 0\n";
                    ++i;
                } else {
                    OUT << "push 0\n";
                }
            }
            scope.addPlaceholder(returnSize);

            // call the function
            OUT << "call " << destFunc.labelName << '\n';
            resultType = destFunc.returnType;
            break;
        }
        case ASTNodeType::ARR_SUBSCRIPT: { // passthrough subscript's type
            if (resultTypes[0] != Type(TokenType::TYPE_INT)) // force int type
                throw TInvalidOperationException(bodyNode.err);
            resultType = resultTypes[0];
            OUT << ";;;;;;;;\n";
            break;
        }
        case ASTNodeType::EXPR: { // top-most expression, pass through
            resultType = resultTypes[0];
            break;
        }
        case ASTNodeType::LIT_ARR: { // pass through
            resultType = static_cast<ASTArrayLiteral*>(&bodyNode)->getTypeRef();
            break;
        }
        default: {
            throw std::invalid_argument("Invalid node type in assembleExpression!");
        }
    }

    // implicit cast
    if (resultType != desiredType) {
        implicitCast(outHandle, resultType, desiredType, scope, bodyNode.err);
        resultType = desiredType; // update type to match
    }

    // result type now matches desired type
    return resultType;
}

// implicitly converts a value pushed to the top of the stack to the given type
void implicitCast(std::ofstream& outHandle, Type resultType, const Type& desiredType, Scope& scope, const ErrInfo err) {
    // most importantly, if the two types are equal just return
    if (resultType == desiredType) return;

    // if both pointers, just pass through (the address doesn't change)
    if (resultType.isPointer() && desiredType.isPointer()) return;

    // if converting between unsigned and signed of same primitive, pass through
    TokenType primA = resultType.getPrimitiveType();
    TokenType primB = desiredType.getPrimitiveType();
    if (primA == primB && !resultType.isPointer() && !desiredType.isPointer() &&
        resultType.isUnsigned() != desiredType.isUnsigned()) return;

    // if converting from a pointer of some sort to an integral type
    // pass through since they share the same size & everything
    if (!desiredType.isPointer() && resultType.isPointer() &&
        desiredType.getPrimitiveType() != TokenType::TYPE_FLOAT &&
        desiredType.getPrimitiveType() != TokenType::VOID) {
        return;
    }

    // if converting from an integral type to a pointer,
    // pass through since they share the same size & everything
    if (!resultType.isPointer() && desiredType.isPointer() &&
        resultType.getPrimitiveType() != TokenType::TYPE_FLOAT &&
        resultType.getPrimitiveType() != TokenType::VOID) {
        return;
    }

    // if converting between primitive types, pad/pop bytes
    if (primA != primB && !resultType.isPointer() && !desiredType.isPointer()) {
        // handle cast to float vs other integral types
        if (primB == TokenType::TYPE_FLOAT) {
            throw std::runtime_error("Float implicit casting not yet implemented!");
        } else if (primA == TokenType::TYPE_FLOAT) {
            throw std::runtime_error("Float implicit casting not yet implemented!");
        } else {
            // converting between two integral types (just pad/pop bytes)
            const size_t startSize = getSizeOfType(primA);
            const size_t endSize = getSizeOfType(primB);

            if (startSize < endSize) { // pad bytes
                for (size_t i = startSize; i < endSize; ++i) {
                    if (i+1 < endSize) {
                        OUT << "pushw 0\n";
                        ++i;
                    } else {
                        OUT << "push 0\n";
                    }
                }
                scope.addPlaceholder(endSize - startSize);
            } else if (startSize > endSize) { // pop bytes
                for (size_t i = startSize; i > endSize; --i) {
                    if (i-1 > endSize) {
                        OUT << "popw\n";
                        --i;
                        scope.pop();
                    } else {
                        OUT << "pop\n";
                    }
                    scope.pop();
                }
            }
        }

        return;
    }

    // base case, illegal cast
    throw TIllegalImplicitCastException(err);
}