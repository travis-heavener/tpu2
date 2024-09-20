#include <fstream>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "ast/ast.hpp"
#include "util/util.hpp"

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
    const size_t returnSize = funcNode.getReturnType().getStackSizeBytes();
    if (funcName == FUNC_MAIN_LABEL && returnSize > 0)
        outHandle << TAB << "add SP, " << returnSize << '\n';

    // add function args to scope (args on top of stack below return bytes)
    for (size_t i = 0; i < funcNode.getNumParams(); i++) { // add the variable to the scope
        ASTFuncParam& arg = *funcNode.paramAt(i);
        // doesn't currently support implied array dimensions in arguments
        if (arg.type.hasEmptyArrayModifiers())
            throw TSyntaxException(funcNode.err);
        scope.declareVariable(arg.type, arg.name, funcNode.err);
    }

    // add return bytes to scope
    if (funcNode.getReturnType().getStackSizeBytes() > 0)
        scope.declareVariable(funcNode.getReturnType(), SCOPE_RETURN_START, funcNode.err);

    // assemble body content
    bool hasReturned = assembleBody(&funcNode, outHandle, scope, funcName, true, true);

    // declare end of function label
    if (!hasReturned && returnSize > 0)
        throw std::invalid_argument("Missing return statement in non-void function: " + funcName);
    outHandle << TAB << "jmp " << labelName + FUNC_END_LABEL_SUFFIX << '\n';

    // write return label
    outHandle << TAB << labelName + FUNC_END_LABEL_SUFFIX << ":\n";

    // stop clock after execution is done IF MAIN or return to previous label
    if (funcName == FUNC_MAIN_LABEL) {
        // handle return status
        outHandle << TAB << "movw AX, 0x03\n"; // specify syscall type
        outHandle << TAB << "popw BX\n"; // pop return status to AX
        outHandle << TAB << "syscall\n"; // trigger syscall
        outHandle << TAB << "hlt\n";
    } else { // return to previous label from call instruction
        outHandle << TAB << "ret\n";
    }
}

// for assembling body content that may or may not have its own scope
// returns true if the current body has returned (really only matters in function scopes)
bool assembleBody(ASTNode* pHead, std::ofstream& outHandle, Scope& scope, const std::string& funcName, const bool isNewScope, const bool isTopScope) {
    size_t startingScopeSize = scope.size(); // remove any scoped variables at the end
    const size_t returnSize = labelMap[funcName].returnType.getStackSizeBytes();
    const std::string returnLabel = labelMap[funcName].returnLabel;

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
                outHandle << TAB << loopStartLabel << ":\n";

                // check condition
                size_t resultSize = assembleExpression(*loop.pExpr, outHandle, scope);

                // load result to AL/AX
                if (resultSize == 2) {
                    outHandle << TAB << "pop AX\n"; // load value to AH
                    scope.pop();
                } else {
                    outHandle << TAB << "xor AH, AH\n"; // clear AH if no value is there
                    outHandle << TAB << "pop AL\n";
                }
                scope.pop();
                outHandle << TAB << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                outHandle << TAB << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                bool hasReturnedLocal = assembleBody(&loop, outHandle, scope, funcName);
                hasReturned = !isTopScope && hasReturnedLocal;

                // jump back to the loopStartLabel
                outHandle << TAB << "jmp " << loopStartLabel << '\n';

                // add merge label
                outHandle << TAB << mergeLabel << ":\n";
                break;
            }
            case ASTNodeType::FOR_LOOP: {
                ASTForLoop& loop = *static_cast<ASTForLoop*>(&child);

                // assemble first expression in current scope
                size_t resultSize = assembleExpression(*loop.pExprA, outHandle, scope);

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    if (j+1 < resultSize) {
                        outHandle << TAB << "popw\n";
                        j++;
                        scope.pop(); // additional pop
                    } else {
                        outHandle << TAB << "pop\n";
                    }
                    scope.pop();
                }

                // create label for the start of the loop body (here)
                const std::string loopStartLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // determine the label where all branches merge
                const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                // append loopStartLabel
                outHandle << TAB << loopStartLabel << ":\n";

                // check condition
                resultSize = assembleExpression(*loop.pExprB, outHandle, scope);

                // load result to AL/AX
                if (resultSize > 1) {
                    outHandle << TAB << "pop AH\n"; // load value to AH
                    scope.pop();
                } else {
                    outHandle << TAB << "xor AH, AH\n"; // clear AH if no value is there
                }
                outHandle << TAB << "pop AL\n";
                scope.pop();
                outHandle << TAB << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                outHandle << TAB << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                bool hasReturnedLocal = assembleBody(&loop, outHandle, scope, funcName);
                hasReturned = !isTopScope && hasReturnedLocal;

                // assemble third expression
                resultSize = assembleExpression(*loop.pExprC, outHandle, scope);

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    if (j+1 < resultSize) {
                        outHandle << TAB << "popw\n";
                        j++;
                        scope.pop(); // additional pop
                    } else {
                        outHandle << TAB << "pop\n";
                    }
                    scope.pop();
                }

                // jump back to the loopStartLabel
                outHandle << TAB << "jmp " << loopStartLabel << '\n';

                // add merge label
                outHandle << TAB << mergeLabel << ":\n";
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

                        assembleExpression(*pExpr, outHandle, scope, getSizeOfType(TokenType::TYPE_BOOL));

                        // pop the result off the stack to AL
                        outHandle << TAB << "xor AH, AH\n"; // clear AH since no value is put there
                        outHandle << TAB << "pop AL\n"; // pop to AL
                        scope.pop();

                        // if false, jump to next condition
                        outHandle << TAB << "buf AX\n"; // set ZF if false
                        outHandle << TAB << "jz " << nextLabel << '\n';
                    }

                    // this is executed when not jumping anywhere (else branch or false condition)
                    // process the body in a new scope
                    bool hasReturnedLocal = assembleBody(conditional.at(j), outHandle, scope, funcName);
                    hasReturned = !isTopScope && hasReturnedLocal;

                    // jump to merge label
                    outHandle << TAB << "jmp " << mergeLabel << '\n';

                    // write next label
                    outHandle << TAB << nextLabel << ":\n";
                }

                break;
            }
            case ASTNodeType::VAR_DECLARATION: {
                // create space on stack
                ASTVarDeclaration& varChild = *static_cast<ASTVarDeclaration*>(&child);
                const Type varType = varChild.getType();
                const size_t typeSize = varType.getStackSizeBytes();

                // get the value of the assignment
                if (varChild.pExpr == nullptr) { // no assignment, set to 0
                    for (size_t j = 0; j < typeSize; j++) {
                        if (j+1 < typeSize) {
                            outHandle << TAB << "pushw 0\n";
                            ++j;
                        } else {
                            outHandle << TAB << "push 0\n";
                        }
                    }
                } else { // has assignment, assemble its expression
                    assembleExpression(*varChild.pExpr, outHandle, scope, typeSize);

                    // remove any placeholders
                    for (size_t j = 0; j < typeSize; j++) scope.pop();
                }

                // add variable to scope
                scope.declareVariable(varType, varChild.pIdentifier->raw, varChild.pIdentifier->err);
                break;
            }
            case ASTNodeType::RETURN: {
                // assemble expression (first and only child of retNode is an ASTExpr*)
                ASTReturn& retNode = *static_cast<ASTReturn*>(&child);
                size_t resultSize = assembleExpression(*retNode.at(0), outHandle, scope, returnSize);

                // move result bytes to their place earlier on the stack
                for (size_t j = 0; j < resultSize; j++) {
                    // pop top of stack into DL
                    outHandle << TAB << "pop DL\n";
                    scope.pop();

                    // mov DL to return bytes location
                    size_t index = scope.getOffset(SCOPE_RETURN_START, retNode.err) - (resultSize - 1 - j);
                    outHandle << TAB << "mov [SP-" << index << "], DL\n";
                }

                // jmp to the end label to finish closing the function
                hasReturned = true;
                break;
            }
            case ASTNodeType::EXPR: {
                // assemble expression
                size_t resultSize = assembleExpression(child, outHandle, scope);

                // nothing from the expression is handled so pop the result off the stack
                for (size_t j = 0; j < resultSize; j++) {
                    if (j+1 < resultSize) {
                        outHandle << TAB << "popw\n";
                        scope.pop(); // additional pop
                        ++j;
                    } else {
                        outHandle << TAB << "pop\n";
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
                outHandle << TAB << "popw\n";
                ++i;
            } else {
                outHandle << TAB << "pop\n";
            }
        }
    }
    
    return hasReturned;
}

// assembles an expression, returning the number of bytes the result uses on the stack
size_t assembleExpression(ASTNode& bodyNode, std::ofstream& outHandle, Scope& scope, long long desiredSize) {
    // if this is a literal array without a type (ie. not part of an assignment), yell at the user (LOUDLY)
    // literal arrays are ONLY allowed during assignment
    if (bodyNode.getNodeType() == ASTNodeType::LIT_ARR) {
        ASTArrayLiteral* pArr = static_cast<ASTArrayLiteral*>(&bodyNode);
        if (pArr->getType().getPrimitiveType() == TokenType::VOID) // unset
            throw TSyntaxException(bodyNode.err);
    }

    // get the desired size of any subexpression, in bytes
    size_t desiredSubSize;
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::LIT_ARR: {
            ASTArrayLiteral& arr = *static_cast<ASTArrayLiteral*>(&bodyNode);
            Type subType = arr.getType();
            subType.popArrayModifier();
            desiredSubSize = subType.getStackSizeBytes(); // the size of each element
            break;
        }
        case ASTNodeType::EXPR: {
            ASTExpr& expr = *static_cast<ASTExpr*>(&bodyNode);
            desiredSubSize = expr.type.getStackSizeBytes();
            break;
        }
        default: desiredSubSize = -1; break;
    }

    // recurse this expression's children, bottom-up
    size_t numChildren = bodyNode.size();
    std::vector<size_t> resultSizes;
    for (size_t i = 0; i < numChildren; ++i) {
        // prevent parsing typecasts
        if (bodyNode.at(i)->getNodeType() == ASTNodeType::TYPE_CAST) continue;

        resultSizes.push_back( assembleExpression(*bodyNode.at(i), outHandle, scope, desiredSubSize) );
    }

    // assemble this node
    long long finalResultSize = 0;
    bodyNode.isAssembled = true;
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::UNARY_OP: {
            ASTOperator& unaryOp = *static_cast<ASTOperator*>(&bodyNode);

            // move argument into AX
            if (resultSizes.size() != 1)
                throw std::runtime_error("Invalid number of resultSizes, expected 1 for unary operation.");

            // ignore OP_ADD since it's really a buffer (passthrough)
            if (unaryOp.getOpTokenType() == TokenType::OP_ADD) {
                finalResultSize = resultSizes[0];
                break;
            }

            // pop in reverse (higher first, later first)
            const size_t resultSize = resultSizes[0];
            if (resultSize == 2) {
                outHandle << TAB << "popw AX\n";
                scope.pop(); // additional pop
            } else {
                outHandle << TAB << "xor AH, AH\n"; // zero out AH
                outHandle << TAB << "pop AL\n";
            }
            scope.pop();

            const std::string regA = resultSize == 1 ? "AL" : "AX";

            switch (unaryOp.getOpTokenType()) {
                case TokenType::OP_SUB: {
                    // negate AX (A ^ 0b10000000 flips sign bit)
                    outHandle << TAB << "xor " << regA << ", " << (resultSize > 1 ? "0x8000" : "0x80") << '\n';
                    
                    // push values to stack
                    if (resultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = resultSize;
                    break;
                }
                case TokenType::OP_BIT_NOT: {
                    // flip all bits
                    outHandle << TAB << "not " << regA << '\n';
                    
                    // push values to stack
                    if (resultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = resultSize;
                    break;
                }
                case TokenType::OP_BOOL_NOT: {
                    // set non-zero to zero, zero to 1
                    const std::string labelZero = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerge = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    outHandle << TAB << "add " << regA << ", 0\n";
                    outHandle << TAB << "jz " << labelZero << '\n'; // zero, set to 1
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n"; // currently non-zero, set to 0
                    else                outHandle << TAB << "mov " << regA << ", 0\n"; // currently non-zero, set to 0
                    outHandle << TAB << "jmp " << labelMerge << '\n'; // reconvene branches

                    outHandle << TAB << labelZero << ":\n"; // currently zero, set to 1
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n";
                    else                outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelMerge << '\n'; // reconvene branches
                    outHandle << TAB << labelMerge << ":\n"; // reconvene branches

                    // push values to stack
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                default: {
                    // handle typecast unary
                    if (unaryOp.getUnaryType() == ASTUnaryType::TYPE_CAST) {
                        // typecast the value presented (for now, just adjust the size of bytes)
                        ASTTypeCast& typeCast = *static_cast<ASTTypeCast*>(unaryOp.left());
                        
                        if (desiredSize == -1)
                            desiredSize = typeCast.type.getStackSizeBytes();

                        // buffer the value to the stack
                        if (resultSize == 2) {
                            outHandle << TAB << "pushw AX\n";
                            scope.addPlaceholder();
                        } else {
                            outHandle << TAB << "push AL\n";
                        }
                        scope.addPlaceholder();
                        finalResultSize = resultSize;
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
            if (resultSizes.size() != 2)
                throw std::runtime_error("Invalid number of resultSizes, expected 2 for binary operation.");
            unsigned char maxResultSize = std::max(resultSizes[0], resultSizes[1]);

            // pop in reverse (higher first, later first)
            if (maxResultSize < 1 || maxResultSize > 2) throw TSyntaxException(bodyNode.err);

            if (maxResultSize == 2) {
                if (resultSizes[1] == 2) { // there's a value here
                    outHandle << TAB << "popw BX\n";
                    scope.pop(); // additional pop
                } else { // no value, but zero the top half of the register to prevent miscalculations
                    outHandle << TAB << "xor BH, BH\n";
                    outHandle << TAB << "pop BL\n";
                }
            } else {
                outHandle << TAB << "pop BL\n";
            }
            scope.pop();

            // ignore popping to the AL/AX register if nothing was pushed from the stack (for assignments)
            if (resultSizes[0] > 0) {
                if (maxResultSize == 2) {
                    if (resultSizes[0] == 2) { // there's a value here
                        outHandle << TAB << "popw AX\n";
                        scope.pop(); // additional pop
                    } else { // no value, but zero the top half of the register to prevent miscalculations
                        outHandle << TAB << "xor AH, AH\n";
                        outHandle << TAB << "pop AL\n";
                    }
                } else {
                    outHandle << TAB << "pop AL\n";
                }
                scope.pop();
            }

            const std::string regA = maxResultSize == 1 ? "AL" : "AX";
            const std::string regB = maxResultSize == 1 ? "BL" : "BX";

            switch (binOp.getOpTokenType()) {
                case TokenType::OP_ADD: {
                    // add AX/AL to BX/BL
                    outHandle << TAB << "add " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_SUB: {
                    // sub AX/AL to BX/BL
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::ASTERISK: {
                    // mul by BX/BL
                    outHandle << TAB << "mul " << regB << '\n';

                    // push result to stack (lowest-first)
                    // max size is 16-bit so just ignore overflow
                    outHandle << TAB << "pushw AX\n";
                    scope.addPlaceholder();
                    scope.addPlaceholder();
                    finalResultSize = 2;
                    break;
                }
                case TokenType::OP_DIV: {
                    // div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) { // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_MOD: {
                    // get mod from div by BX/BL
                    outHandle << TAB << "div " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) { // uses AX as result and DX as remainder in 16-bit mode
                        outHandle << TAB << "pushw DX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AH\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_BIT_OR: {
                    outHandle << TAB << "or " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_BIT_AND: {
                    outHandle << TAB << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_BIT_XOR: {
                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_BOOL_OR: {
                    outHandle << TAB << "or " << regA << ", " << regB << '\n';

                    // if ZF is cleared, expression is true (set to 1)
                    const std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n";
                    else                outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_BOOL_AND: {
                    // if either one is zero, boolean and is false

                    // put 0 into regA if zero, 1 otherwise
                    // if ZF is set, set regA (non-zero -> 1)
                    std::string labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "or " << regA << ", 0\n"; // sets ZF if 0
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n";
                    else                outHandle << TAB << "mov " << regA << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // put 0 into regB if zero, 1 otherwise
                    // if ZF is set, set regB (non-zero -> 1)
                    labelName = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "or " << regB << ", 0\n"; // sets ZF if 0
                    outHandle << TAB << "jz " << labelName << '\n'; // skip over assignment to 1
                    if (regB[1] == 'X')
                        outHandle << TAB << "movw " << regB << ", 1\n";
                    else
                        outHandle << TAB << "mov " << regB << ", 1\n";
                    outHandle << TAB << "jmp " << labelName << '\n'; // reconvene with other branch
                    outHandle << TAB << labelName << ":\n"; // reconvene with other branch

                    // push regA & regB (0b0 & 0b1 or some combination)
                    outHandle << TAB << "and " << regA << ", " << regB << '\n';

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_EQ: {
                    // if A ^ B is zero, equal
                    const std::string labelEQ = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';

                    outHandle << TAB << "jz " << labelEQ << '\n';
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n"; // non-zero becomes 0
                    else                outHandle << TAB << "mov " << regA << ", 0\n"; // non-zero becomes 0
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    outHandle << TAB << labelEQ << ":\n";
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // zero becomes 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // zero becomes 1
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_NEQ: {
                    // if A ^ B is zero, equal (keep as 0)
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);

                    outHandle << TAB << "xor " << regA << ", " << regB << '\n';
                    outHandle << TAB << "jz " << labelMerger << '\n';
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // non-zero becomes 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // non-zero becomes 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_LT: {
                    // if A < B, B-A will have carry and zero cleared
                    outHandle << TAB << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelGTE << '\n'; // zero is set, A == B
                    outHandle << TAB << "jc " << labelGTE << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // set regA to 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    outHandle << TAB << labelGTE << ":\n"; // set regA to 0
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n";
                    else                outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    /// push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_GT: {
                    // if A > B, A-B will have carry and zero cleared
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jz " << labelLTE << '\n'; // zero is set, A == B
                    outHandle << TAB << "jc " << labelLTE << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // set regA to 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelLTE << ":\n"; // set regA to 0
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n";
                    else                outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_LTE: {
                    // if A <= B, B-A will have carry cleared
                    outHandle << TAB << "sub " << regB << ", " << regA << '\n';

                    const std::string labelGT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jc " << labelGT << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // set regA to 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelGT << ":\n"; // set regA to 0
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n"; // set regA to 1
                    else                outHandle << TAB << "mov " << regA << ", 0\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_GTE: {
                    // if A >= B, A-B will have carry cleared
                    outHandle << TAB << "sub " << regA << ", " << regB << '\n';

                    const std::string labelLT = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    outHandle << TAB << "jc " << labelLT << '\n'; // carry is set, A > B
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 1\n"; // set regA to 1
                    else                outHandle << TAB << "mov " << regA << ", 1\n"; // set regA to 1
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelLT << ":\n"; // set regA to 0
                    if (regA[1] == 'X') outHandle << TAB << "movw " << regA << ", 0\n";
                    else                outHandle << TAB << "mov " << regA << ", 0\n";
                    outHandle << TAB << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    outHandle << TAB << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    outHandle << TAB << "push AL\n";
                    scope.addPlaceholder();
                    finalResultSize = 1; // always returns an 8-bit bool
                    break;
                }
                case TokenType::OP_LSHIFT: {
                    // can only use 8-bit register for shift count
                    outHandle << TAB << "shl " << regA << ", BL\n";

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::OP_RSHIFT: {
                    // can only use 8-bit register for shift count
                    outHandle << TAB << "shr " << regA << ", BL\n";

                    // push result to stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw AX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push AL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = maxResultSize;
                    break;
                }
                case TokenType::ASSIGN: {
                    // get the stack offset of the variable
                    if (bodyNode.at(0)->getNodeType() != ASTNodeType::IDENTIFIER)
                        throw TSyntaxException(bodyNode.at(0)->err);

                    ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(bodyNode.at(0));
                    ScopeAddr* pScopeVar = scope.getVariable(identifier.raw, identifier.err);
                    size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);
                    size_t typeSize = pScopeVar->type.getStackSizeBytes();

                    // prevent void assignment
                    if (resultSizes[1] == 0) throw TVoidReturnException(identifier.err);

                    // move the rvalue to the identifier on the left
                    outHandle << TAB << "mov [SP-" << stackOffset << "], BL" << '\n';
                    if (resultSizes[1] == 2)
                        outHandle << TAB << "mov [SP-" << stackOffset-1 << "], BH" << '\n';

                    // push the value of the variable onto the stack (lowest-first)
                    if (maxResultSize == 2) {
                        outHandle << TAB << "pushw BX\n";
                        scope.addPlaceholder(); // additional placeholder
                    } else {
                        outHandle << TAB << "push BL\n";
                    }
                    scope.addPlaceholder();
                    finalResultSize = typeSize;
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
            size_t size = getSizeOfType(TokenType::TYPE_INT);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                if (i+1 < size) {
                    outHandle << TAB << "pushw " << (value & 0xFFFF) << '\n';
                    scope.addPlaceholder(); // additional placeholder
                    i++;
                } else {
                    outHandle << TAB << "push " << (value & 0xFF) << '\n';
                }
                scope.addPlaceholder();
                value >>= 8; // shift downward
            }
            finalResultSize = size;
            break;
        }
        case ASTNodeType::LIT_BOOL: {
            // get value & size of literal
            bool value = static_cast<ASTBoolLiteral*>(&bodyNode)->val;
            size_t size = getSizeOfType(TokenType::TYPE_BOOL);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                if (i+1 < size) {
                    outHandle << TAB << "pushw " << (value & 0xFFFF) << '\n';
                    scope.addPlaceholder(); // additional placeholder
                    i++;
                } else {
                    outHandle << TAB << "push " << (value & 0xFF) << '\n';
                }
                scope.addPlaceholder();
                value >>= 8; // shift downward
            }
            finalResultSize = size;
            break;
        }
        case ASTNodeType::LIT_CHAR: {
            // get value & size of literal
            char value = static_cast<ASTCharLiteral*>(&bodyNode)->val;
            size_t size = getSizeOfType(TokenType::TYPE_CHAR);
            
            // push primitive to stack
            for (size_t i = 0; i < size; i++) {
                if (i+1 < size) {
                    outHandle << TAB << "pushw " << (value & 0xFFFF) << '\n';
                    scope.addPlaceholder(); // additional placeholder
                    i++;
                } else {
                    outHandle << TAB << "push " << (value & 0xFF) << '\n';
                }
                scope.addPlaceholder();
                value >>= 8; // shift downward
            }
            finalResultSize = size;
            break;
        }
        case ASTNodeType::LIT_FLOAT: {
            throw std::invalid_argument("Float arithmetic not implemented yet!");
        }
        case ASTNodeType::IDENTIFIER: {
            // lookup identifier
            ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(&bodyNode);
            ScopeAddr* pScopeVar = scope.getVariable(identifier.raw, identifier.err);
            size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);

            // apply subscripts if necessary
            size_t typeSize = pScopeVar->type.getStackSizeBytes();
            if (identifier.getSubscripts().size() > 0) {
                // move the SP to the BP to store the stackOffset's offset
                outHandle << TAB << "movw BP, SP\n";

                // update stack offset
                const std::vector<long long>& stackMods = pScopeVar->type.getArrayModifiers();
                const std::vector<ASTArraySubscript*>& subscripts = identifier.getSubscripts();

                // assemble subscripts to get their result (****subscripts are ints)
                size_t k = 0;
                for (ASTArraySubscript* pSub : subscripts) {
                    // get size of each subtype we're indexing
                    typeSize /= stackMods[ k++ ];

                    // add to BP the top int value on the stack (force subscript size to int)
                    assembleExpression(*pSub, outHandle, scope, getSizeOfType(TokenType::TYPE_INT));

                    // pop top int (subscript) to AX
                    outHandle << TAB << "popw AX\n";
                    scope.pop(); scope.pop();

                    outHandle << TAB << "movw BX, " << typeSize << '\n'; // move typeSize to BX
                    outHandle << TAB << "mul BX\n"; // scale AX by BX
                    outHandle << TAB << "add BP, AX\n"; // add to BP
                }

                // handle assignment operations vs read operations
                if (!identifier.isInAssignExpr) { // read operation, so push the value of the identifier onto the stack
                    for (size_t i = 0; i < typeSize; i++) {
                        // move value to top of stack
                        // use stackOffset-i since the stack is changing but BP is not
                        outHandle << TAB << "push [BP-" << stackOffset-i << "]\n";
                        scope.addPlaceholder();
                    }
                    finalResultSize = typeSize;
                    break;
                }
            } else {
                // handle assignment operations vs read operations
                if (!identifier.isInAssignExpr) { // read operation, so push the value of the identifier onto the stack
                    for (size_t i = 0; i < typeSize; i++) {
                        // move value to top of stack
                        outHandle << TAB << "push [SP-" << stackOffset << "]\n";
                        scope.addPlaceholder();
                    }
                    finalResultSize = typeSize;
                    break;
                }
            }

            // base case, assignment operation, push nothing to the stack
            finalResultSize = 0;
            break;
        }
        case ASTNodeType::FUNCTION_CALL: {
            // lookup the function
            ASTFunctionCall& func = *static_cast<ASTFunctionCall*>(&bodyNode);
            const std::string funcName = func.raw;
            if (labelMap.count(funcName) == 0) throw TUnknownIdentifierException(func.err);

            // allocate space on the stack for return bytes
            assembled_func_t destFunc = labelMap[funcName];
            size_t returnSize = destFunc.returnType.getStackSizeBytes();

            for (size_t i = 0; i < returnSize; i++) {
                if (i+1 < returnSize) {
                    outHandle << TAB << "pushw 0\n";
                    scope.addPlaceholder();
                    ++i;
                } else {
                    outHandle << TAB << "push 0\n";
                }
                scope.addPlaceholder();
            }

            // call the function
            outHandle << TAB << "call " << destFunc.labelName << '\n';
            finalResultSize = returnSize;
            break;
        }
        case ASTNodeType::ARR_SUBSCRIPT: {
            // passthrough the size of the subscript
            desiredSize = getSizeOfType(TokenType::TYPE_INT); // force int size
            finalResultSize = resultSizes[0];
            break;
        }
        case ASTNodeType::EXPR: {
            // this is the top-most expression, so just pass through
            finalResultSize = resultSizes[0];
            break;
        }
        case ASTNodeType::LIT_ARR: {
            // children are already on stack, so pass through the size of the node
            ASTArrayLiteral& arr = *static_cast<ASTArrayLiteral*>(&bodyNode);
            Type type = arr.getType();

            // force type-cast all children
            size_t sumResultSize = 0;
            for (size_t resultSize : resultSizes) sumResultSize += resultSize;

            finalResultSize = type.getStackSizeBytes();
            break;
        }
        default: {
            throw std::invalid_argument("Invalid node type in assembleExpression!");
        }
    }

    // cast size, if necessary
    if (desiredSize != -1) {
        // prevent returning void to an expression
        if (finalResultSize == 0 && desiredSize > 0) throw TVoidReturnException(bodyNode.err);

        if (finalResultSize < desiredSize) { // push extra 0s
            for (long long i = finalResultSize; i < desiredSize; i++) {
                if (i+1 < desiredSize) {
                    outHandle << TAB << "pushw 0\n";
                    scope.addPlaceholder();
                    ++i;
                } else {
                    outHandle << TAB << "push 0\n";
                }
                scope.addPlaceholder();
            }

            finalResultSize = desiredSize;
        } else if (finalResultSize > desiredSize) { // pop extra values
            for (long long i = finalResultSize; i > desiredSize; i--) {
                if (i-1 > desiredSize) {
                    outHandle << TAB << "popw\n";
                    scope.pop();
                    --i;
                } else {
                    outHandle << TAB << "pop\n";
                }
                scope.pop();
            }
            
            finalResultSize = desiredSize;
        }
    }

    // output should now be pseudo-cast (I say pseudo because this isn't actually type-casting)
    return finalResultSize;
}