#include <fstream>
#include <string>
#include <vector>

#include "assembler.hpp"
#include "ast/ast.hpp"
#include "util/toolbox.hpp"
#include "util/token.hpp"
#include "util/type.hpp"
#include "util/t_exception.hpp"
#include "util/config.hpp"

// static fields
static size_t nextFuncLabelID = 0;
static size_t nextJMPLabelID = 0;
static size_t nextStringDataID = 0;
static label_map_t labelMap;
static std::vector<DataElem> dataElements;

AssembledFunc::AssembledFunc(const std::string& funcName, const ASTFunction& func) {
    this->funcName = funcName;

    // determine if this is the main function
    this->paramTypes = std::vector<Type>();
    this->returnType = func.getReturnType();
    
    // fill in param types
    func.loadParamTypes(this->paramTypes);

    // determine labels
    this->startLabel = func.isMainFunction() ? RESERVED_LABEL_MAIN : (FUNC_LABEL_PREFIX + std::to_string(nextFuncLabelID++));
    this->endLabel = this->startLabel + FUNC_END_LABEL_SUFFIX;
}

// generate TPU assembly code from the AST
void generateAssembly(AST& ast, std::ofstream& outHandle) {
    // write .text section
    outHandle << "section .text\n";

    // iterate over global functions
    std::vector<ASTNode*>& globalChildren = ast.getChildren();
    for (ASTNode* pFunc : globalChildren) {
        // build list of function labels
        ASTFunction& funcNode = *static_cast<ASTFunction*>(pFunc);

        // assemble function
        assembleFunction(funcNode, outHandle);
    }

    // write .data section
    outHandle << "section .data\n";

    // iterate over all data elements
    size_t dataStrId = 0;
    for (DataElem elem : dataElements) {
        outHandle << TAB << STR_DATA_LABEL_PREFIX << dataStrId++ << ' ' <<
            elem.type << ' ' << elem.raw << '\n';
    }
}

// for assembling functions
void assembleFunction(ASTFunction& funcNode, std::ofstream& outHandle) {
    // determine labelName
    const std::string funcName = funcNode.getName();

    // create new AssembledFunc to store name & return type
    AssembledFunc asmFunc = AssembledFunc(funcName, funcNode);
    labelMap.insert({funcName, asmFunc});

    // create a scope for this body
    Scope scope;

    // assemble this function into the file
    outHandle << asmFunc.getStartLabel() << ":\n";

    // if this is main, add return byte spacing on top of stack
    const size_t returnSize = funcNode.getReturnType().getSizeBytes();
    if (asmFunc.getStartLabel() == RESERVED_LABEL_MAIN && returnSize > 0)
        OUT << "add SP, " << returnSize << '\n';

    // add return bytes to scope
    if (funcNode.getReturnType().getSizeBytes() > 0)
        scope.declareVariable(funcNode.getReturnType(), SCOPE_RETURN_START, funcNode.err);

    // add function args to scope (args on top of stack just above return bytes)
    for (size_t i = 0; i < funcNode.getNumParams(); ++i) { // add the variable to the scope
        ASTFuncParam* pArg = funcNode.paramAt(i);
        scope.declareFunctionParam(pArg->type, pArg->name, funcNode.err);
    }

    // assemble body content
    bool hasReturned = assembleBody(&funcNode, outHandle, scope, asmFunc, true);

    // declare end of function label
    if (!hasReturned && returnSize > 0)
        throw TMissingReturnException(funcNode.err);
    OUT << "jmp " << asmFunc.getEndLabel() << '\n';

    // write return label
    OUT << asmFunc.getEndLabel() << ":\n";

    // stop clock after execution is done IF MAIN or return to previous label
    if (asmFunc.getStartLabel() == RESERVED_LABEL_MAIN) {
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
bool assembleBody(ASTNode* pHead, std::ofstream& outHandle, Scope& scope, const AssembledFunc& asmFunc, const bool isTopScope) {
    size_t startingScopeSize = scope.size(); // remove any scoped variables at the end
    const Type& desiredType = asmFunc.getReturnType();
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
                    OUT << "pop AL\n";
                    OUT << "xor AH, AH\n"; // clear AH if no value is there
                }
                scope.pop();
                OUT << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                OUT << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                assembleBody(&loop, outHandle, scope, asmFunc);

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
                OUT << "sub SP, " << resultSize << '\n';
                scope.pop(resultSize);

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
                    OUT << "pop AL\n";
                    OUT << "xor AH, AH\n"; // clear AH if no value is there
                }
                scope.pop();
                OUT << "buf AX\n"; // test ZF flag

                // if the result sets the ZF flag, it's false so jmp to mergeLabel
                OUT << "jz " << mergeLabel << "\n";

                // assemble the body here in new scope
                assembleBody(&loop, outHandle, scope, asmFunc);

                // assemble third expression
                resultSize = assembleExpression(*loop.pExprC, outHandle, scope).getSizeBytes();

                // nothing from the expression is handled so pop the result off the stack
                OUT << "sub SP, " << resultSize << '\n';
                scope.pop(resultSize);

                OUT << "jmp " << loopStartLabel << '\n'; // jump back to the loopStartLabel
                OUT << mergeLabel << ":\n"; // add merge label
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
                        OUT << "pop AL\n"; // pop to AL
                        OUT << "xor AH, AH\n"; // clear AH since no value is put there
                        scope.pop();

                        // if false, jump to next condition
                        OUT << "buf AX\n"; // set ZF if false
                        OUT << "jz " << nextLabel << '\n';
                    }

                    // this is executed when not jumping anywhere (else branch or false condition)
                    // process the body in a new scope
                    assembleBody(conditional.at(j), outHandle, scope, asmFunc);

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
                if (pVarChild->pExpr == nullptr) { // no assignment
                    OUT << "add SP, " << typeSize << '\n';
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

                if (retNode.size() > 0) {
                    Type resultType = assembleExpression(*retNode.at(0), outHandle, scope);

                    if (desiredType.isVoidNonPtr() && !resultType.isVoidNonPtr())
                        throw TVoidReturnException(retNode.err);

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
                if (resultSize > 0) {
                    OUT << "sub SP, " << resultSize << '\n';
                    scope.pop(resultSize);
                }
                break;
            }
            default: {
                throw TDevException("Unimplemented ASTNodeType!");
            }
        }
    }

    // remove extra scope variables after the scope closes
    long long sizeFreed = scope.size() - startingScopeSize;

    // move back SP
    if (sizeFreed > 0) {
        OUT << "sub SP, " << sizeFreed << '\n';
        scope.pop(sizeFreed);
    }

    // jump to end if returned
    if (hasReturned) {
        // remove everything else in the scope except the return bytes
        // DON'T USE scope.pop SINCE THIS ISN'T A GUARANTEED RETURN
        if (!isTopScope) {
            size_t argSizes = 0;
            for (const Type& t : asmFunc.getParamTypes())
                argSizes += t.getSizeBytes();

            long long popSize = scope.size() - returnSize - argSizes; // argSizes popped by caller

            if (popSize > 0) OUT << "sub SP, " << popSize << '\n';
        }
        OUT << "jmp " << asmFunc.getEndLabel() << '\n';
    }

    return hasReturned;
}

// assembles an expression, returning the type of the value pushed to the stack
Type assembleExpression(ASTNode& bodyNode, std::ofstream& outHandle, Scope& scope) {
    // if this is a literal array without a type (ie. not part of an assignment), yell at the user (LOUDLY)
    // literal arrays are ONLY allowed during assignment
    if (bodyNode.getNodeType() == ASTNodeType::LIT_ARR) {
        ASTArrayLiteral* pArr = static_cast<ASTArrayLiteral*>(&bodyNode);
        if (pArr->getType().getPrimType() == TokenType::VOID) // unset
            throw TSyntaxException(bodyNode.err);
    }

    // get desired expression type
    const Type desiredType = static_cast<ASTTypedNode*>(&bodyNode)->getType();

    // if this node is a function call, allocate return bytes below args
    if (bodyNode.getNodeType() == ASTNodeType::FUNCTION_CALL) {
        ASTFunctionCall& func = *static_cast<ASTFunctionCall*>(&bodyNode);
        const std::string& funcName = func.raw;

        // find function and verify parameters match
        AssembledFunc* pDestFunc = nullptr;
        const size_t numParams = func.size();
        auto [itr, end] = labelMap.equal_range(funcName);
        for ((void)itr; itr != end; ++itr) {
            // check if parameters match
            const std::vector<Type>& paramTypes = itr->second.getParamTypes();
            if (paramTypes.size() != func.size()) continue;

            size_t j;
            for (j = 0; j < numParams; ++j) {
                const Type& actualType = static_cast<ASTTypedNode*>(func.at(j))->getTypeRef();
                if (paramTypes[j].isParamMatch(actualType, func.at(j)->err) == TYPE_PARAM_MISMATCH) break;
            }

            // if broken prematurely, a type didn't match
            if (j == numParams) {
                pDestFunc = &itr->second;
                break;
            }
        }

        if (pDestFunc == nullptr)
            throw TUnknownIdentifierException(bodyNode.err);

        // allocate space on the stack for return bytes
        size_t returnSize = pDestFunc->getReturnType().getSizeBytes();

        for (size_t i = 0; i < returnSize; i++) {
            if (i+1 < returnSize) {
                OUT << "pushw 0\n";
                ++i;
            } else {
                OUT << "push 0\n";
            }
        }
        scope.addPlaceholder(returnSize);
    }

    // recurse this expression's children, bottom-up
    size_t numChildren = bodyNode.size();
    std::vector<Type> resultTypes;
    for (size_t i = 0; i < numChildren; ++i) {
        resultTypes.push_back( assembleExpression(*bodyNode.at(i), outHandle, scope) );
    }

    // assemble this node
    Type resultType;
    switch (bodyNode.getNodeType()) {
        case ASTNodeType::UNARY_OP: {
            ASTOperator& unaryOp = *static_cast<ASTOperator*>(&bodyNode);

            // passthrough nullified operators
            if (unaryOp.isNullified()) {
                resultType = desiredType;
                break;
            }

            // move argument into AX
            if (resultTypes.size() != 1)
                throw TDevException("Invalid number of resultTypes, expected 1 for unary operation.");

            // ignore OP_ADD since it's really a buffer (passthrough)
            if (unaryOp.getOpTokenType() == TokenType::OP_ADD) {
                resultType = resultTypes[0];
                break;
            }

            // pop in reverse (higher first, later first)
            size_t resultSize = resultTypes[0].getSizeBytes(SIZE_ARR_AS_PTR);
            if (resultSize == 2) {
                OUT << "popw AX\n";
                scope.pop(); // additional pop
            } else if (resultSize == 1) {
                OUT << "pop AL\n";
                OUT << "xor AH, AH\n"; // zero out AH
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
                    OUT << "not " << regA << '\n'; // flip all bits
                    if (opType == TokenType::OP_SUB) // add 1 (2's complement)
                        OUT << "add " << regA << ", 1\n";

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
                    OUT << "movw BP, AX\n"; // dereference ptr whose address is stored in AX

                    // pass final result size
                    resultType = resultTypes[0];
                    resultType.popPointer();

                    // if lvalue OR this is a pointer and the top subscript is an array hint, pass the address
                    if (unaryOp.isLValue() ||
                        (resultType.isPointer() && resultType.getPointers().back() != TYPE_EMPTY_PTR)) {
                        // buffer to stack
                        OUT << "pushw BP\n";
                        scope.addPlaceholder(2);
                    } else {
                        // move that value to the stack
                        for (size_t k = 0; k < resultType.getSizeBytes(); ++k) {
                            OUT << "push [BP+" << k << "]\n";
                            scope.addPlaceholder();
                        }
                    }
                    break;
                }
                case TokenType::AMPERSAND: {
                    // add layer of indirection by pushing/buffering the address
                    OUT << "pushw AX\n";
                    scope.addPlaceholder(2);

                    // update result type
                    resultType = resultTypes[0];
                    resultType.addEmptyPointer();
                    break;
                }
                case TokenType::SIZEOF: {
                    // if this is a pointer TO an array, correct its size
                    if (resultTypes[0].isArray() && resultTypes[0].getPointers().back() == TYPE_EMPTY_PTR) {
                        // this is still just a pointer
                        resultSize = MEM_ADDR_SIZE;
                    } else if (resultTypes[0].isArray()) {
                        resultSize = resultTypes[0].getSizeBytes();
                    }

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
                    throw TDevException("Invalid unaryOp type in assembleExpression!");
                }
            }
            break;
        }
        case ASTNodeType::BIN_OP: {
            ASTOperator& binOp = *static_cast<ASTOperator*>(&bodyNode);
            const TokenType opType = binOp.getOpTokenType();

            // passthrough nullified operators
            if (binOp.isNullified()) {
                resultType = desiredType;
                break;
            }

            // move both arguments into AX & BX
            if (resultTypes.size() != 2)
                throw TDevException("Invalid number of resultSizes, expected 2 for binary operation.");

            // get dominant type
            const Type dominantType = getDominantType(resultTypes[0], resultTypes[1]);
            const size_t dominantSize = dominantType.getSizeBytes(SIZE_ARR_AS_PTR);

            // pop in reverse (higher first, later first)
            if (dominantSize < 1 || dominantSize > 2) throw TSyntaxException(bodyNode.err);

            if (resultTypes[1].getSizeBytes() == 2) { // pop to BX
                OUT << "popw BX\n";
                scope.pop();
            } else { // pop to BL and zero BH
                OUT << "pop BL\n";
                OUT << "xor BH, BH\n";
            }
            scope.pop();

            // ignore popping to the AL/AX register if nothing was pushed from the stack (for assignments)
            if (isTokenAssignOp(opType)) {
                // force assignment to pop the address
                OUT << "popw AX\n";
                scope.pop(2);
            } else {
                if (resultTypes[0].getSizeBytes(SIZE_ARR_AS_PTR) == 2) { // pop to AX
                    OUT << "popw AX\n";
                    scope.pop(2);
                } else { // pop to AL and zero AH
                    OUT << "pop AL\n";
                    OUT << "xor AH, AH\n";
                    scope.pop();
                }
            }

            // determine output registers
            const std::string regA = dominantSize == 1 ? "AL" : "AX";
            const std::string regB = dominantSize == 1 ? "BL" : "BX";

            switch (opType) {
                case TokenType::OP_ADD: case TokenType::OP_SUB: { // add/sub AX/AL to/from BX/BL
                    // handle pointer arithmetic
                    Type typeA = resultTypes[0], typeB = resultTypes[1];
                    const bool isUnsigned = typeA.isUnsigned() || typeB.isUnsigned();

                    if (typeA.isPointer()) {
                        typeA.popPointer(); // get internal size, also removes forced ptr status for accurate size
                        const size_t chunkSize = typeA.getSizeBytes();

                        // mul needs AX register, so move it temporarily (don't need to do scope.pop/addPlaceholder)
                        if (chunkSize > 0) {
                            OUT << "pushw AX\n";
                            OUT << "movw AX, " << chunkSize << '\n';
                            OUT << "mul BX\n"; // other operand is in BX already
                            OUT << "movw BX, AX\n"; // save new operand
                            OUT << "popw AX\n"; // move AX back
                        }
                    } else if (typeB.isPointer()) {
                        typeB.popPointer(); // get internal size, also removes forced ptr status for accurate size
                        const size_t chunkSize = typeB.getSizeBytes();

                        // operand already in AX; move chunk size into CX
                        if (chunkSize > 0) {
                            OUT << "movw CX, " << chunkSize << '\n';
                            OUT << "mul CX\n"; // other operand is in BX already
                        }
                    }

                    // perform add or sub
                    if (isUnsigned) {
                        if (opType == TokenType::OP_ADD) OUT_BIN_OP_2(add);
                        else OUT_BIN_OP_2(sub);
                    } else {
                        if (opType == TokenType::OP_ADD) OUT_BIN_OP_2(sadd);
                        else OUT_BIN_OP_2(ssub);
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
                case TokenType::ASTERISK: case TokenType::OP_DIV: case TokenType::OP_MOD:
                case TokenType::OP_BIT_OR: case TokenType::AMPERSAND: case TokenType::OP_BIT_XOR: {
                    Type typeA = resultTypes[0], typeB = resultTypes[1];
                    const bool isUnsigned = typeA.isUnsigned() || typeB.isUnsigned();

                    // all use similar/the same process, so combined them here
                    if (isUnsigned) {
                        switch (opType) {
                            case TokenType::ASTERISK:   OUT_BIN_OP_1B(mul); break; // mul AX/AL by BX/BL
                            case TokenType::OP_DIV:     OUT_BIN_OP_1B(div); break; // div AX/AL by BX/BL
                            case TokenType::OP_MOD:     OUT_BIN_OP_1B(div); break; // mod AX/AL by BX/BL
                            case TokenType::OP_BIT_OR:  OUT_BIN_OP_2(or); break; // bitwise or
                            case TokenType::AMPERSAND:  OUT_BIN_OP_2(and); break; // bitwise and
                            case TokenType::OP_BIT_XOR: OUT_BIN_OP_2(xor); break; // bitwise xor
                            default: break; // doesn't get here
                        }
                    } else {
                        switch (opType) {
                            case TokenType::ASTERISK:   OUT_BIN_OP_1B(smul); break; // mul AX/AL by BX/BL
                            case TokenType::OP_DIV:     OUT_BIN_OP_1B(sdiv); break; // div AX/AL by BX/BL
                            case TokenType::OP_MOD:     OUT_BIN_OP_1B(sdiv); break; // mod AX/AL by BX/BL
                            case TokenType::OP_BIT_OR:  OUT_BIN_OP_2(or); break; // bitwise or
                            case TokenType::AMPERSAND:  OUT_BIN_OP_2(and); break; // bitwise and
                            case TokenType::OP_BIT_XOR: OUT_BIN_OP_2(xor); break; // bitwise xor
                            default: break; // doesn't get here
                        }
                    }

                    // push result to stack (lowest-first)
                    if (opType == TokenType::OP_MOD) // uses AX as result and DX as remainder in 16-bit mode
                        OUT << (dominantSize == 2 ? "pushw DX\n" : "push AH\n");
                    else
                        OUT << (dominantSize == 2 ? "pushw AX\n" : "push AL\n");

                    // get the sign from DH for 16-bit mult
                    if (opType == TokenType::ASTERISK && dominantSize == 2 && !isUnsigned) {
                        // grab sign from DH
                        OUT << "pop CH\n";
                        OUT << "mov CL, DH\n";
                        OUT << "and CL, 128\n";
                        OUT << "or CH, CL\n";
                        OUT << "push CH\n";
                    }

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
                    // if ZF is set, equal
                    const std::string labelEQ = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "cmp " << regA << ", " << regB << '\n';

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
                    // if A < B, CF will be set
                    const bool isUnsigned = resultTypes[0].isUnsigned() || resultTypes[1].isUnsigned();
                    OUT << (isUnsigned ? "cmp " : "scmp ") << regA << ", " << regB << '\n';

                    const std::string labelGTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jnc " << labelGTE << '\n';
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
                    // if A > B, CF will be set
                    const bool isUnsigned = resultTypes[0].isUnsigned() || resultTypes[1].isUnsigned();
                    OUT << (isUnsigned ? "cmp " : "scmp ") << regB << ", " << regA << '\n';

                    const std::string labelLTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jnc " << labelLTE << '\n';
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
                    // if A > B, CF will be set
                    const bool isUnsigned = resultTypes[0].isUnsigned() || resultTypes[1].isUnsigned();
                    OUT << (isUnsigned ? "cmp " : "scmp ") << regB << ", " << regA << '\n';

                    const std::string labelLTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jnc " << labelLTE << '\n';
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n"; // set regA to 0
                    else                OUT << "mov " << regA << ", 0\n"; // set regA to 0
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    OUT << labelLTE << ":\n"; // set regA to 1
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n";
                    else                OUT << "mov " << regA << ", 1\n";
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch
                    OUT << labelMerger << ":\n"; // reconvene with other branch

                    // push result to stack (lowest-first)
                    BIN_OP_RECORD_BOOL;
                    break;
                }
                case TokenType::OP_GTE: {
                    // if A < B, CF will be set
                    const bool isUnsigned = resultTypes[0].isUnsigned() || resultTypes[1].isUnsigned();
                    OUT << (isUnsigned ? "cmp " : "scmp ") << regA << ", " << regB << '\n';

                    const std::string labelGTE = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    const std::string labelMerger = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "jnc " << labelGTE << '\n';
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 0\n"; // set regA to 0
                    else                OUT << "mov " << regA << ", 0\n"; // set regA to 0
                    OUT << "jmp " << labelMerger << '\n'; // reconvene with other branch

                    OUT << labelGTE << ":\n"; // set regA to 1
                    if (regA[1] == 'X') OUT << "movw " << regA << ", 1\n";
                    else                OUT << "mov " << regA << ", 1\n";
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
                    const size_t rvalueSize = resultTypes[1].getSizeBytes();
                    if (rvalueSize == 2) {
                        OUT << "mov [BP+0], BL" << '\n';
                        OUT << "mov [BP+1], BH" << '\n';
                    } else {
                        OUT << "mov [BP+0], BL" << '\n';
                    }

                    // push the value of the variable onto the stack (lowest-first)
                    OUT << (rvalueSize == 2 ? "pushw BX\n" : "push BL\n");
                    scope.addPlaceholder(rvalueSize);
                    resultType = resultTypes[1];
                    break;
                }
                default:
                    throw TDevException("Invalid binOp type in assembleExpression!");
            }
            break;
        }
        case ASTNodeType::LIT_INT: {
            // get value & size of literal
            ASTIntLiteral* pLit = static_cast<ASTIntLiteral*>(&bodyNode);
            unsigned short value = pLit->val & 0xFFFF;
            resultType = Type(TokenType::TYPE_INT);
            OUT << "pushw " << value << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_BOOL: {
            // get value & size of literal
            ASTBoolLiteral* pLit = static_cast<ASTBoolLiteral*>(&bodyNode);
            unsigned short value = pLit->val & 0xFF;
            resultType = Type(TokenType::TYPE_BOOL);
            OUT << "push " << value << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_CHAR: {
            // get value & size of literal
            ASTCharLiteral* pLit = static_cast<ASTCharLiteral*>(&bodyNode);
            unsigned short value = pLit->val & 0xFF;
            resultType = Type(TokenType::TYPE_CHAR);
            OUT << "push " << value << '\n';
            scope.addPlaceholder(resultType.getSizeBytes());
            break;
        }
        case ASTNodeType::LIT_FLOAT: {
            throw TDevException("Float arithmetic not implemented yet!");
        }
        case ASTNodeType::IDENTIFIER: {
            // lookup identifier
            ASTIdentifier& identifier = *static_cast<ASTIdentifier*>(&bodyNode);
            size_t stackOffset = scope.getOffset(identifier.raw, identifier.err);
            Type idenType = scope.getVariable(identifier.raw, identifier.err)->type;

            // if there aren't any subscripts, handle the value here
            if (identifier.getNumSubscripts() == 0) {
                if (idenType.isPointer()) { // handle pointers
                    // push the address onto the stack
                    OUT << "movw BP, SP\n"; // move SP into BP for manipulation w/o affecting the SP
                    OUT << "sub BP, " << stackOffset << '\n';
                    OUT << "pushw BP\n";
                    scope.addPlaceholder(2);

                    // if this is a reference pointer (ie. array passed as an argument), dereference it
                    if (idenType.isReferencePointer()) {
                        // doesn't need to be a reference pointer anymore
                        OUT << "popw BP\n"; // move address back into BP
                        idenType.setIsReferencePointer(false);
                        scope.pop(2);

                        // push the referenced value
                        const size_t typeSize = idenType.getSizeBytes();
                        for (size_t k = 0; k < typeSize; ++k)
                            OUT << "push [BP+" << k << "]\n";
                        scope.addPlaceholder(typeSize);
                    }

                    // if this is an lvalue, leave the address on the stack
                    if (!idenType.isArray() && !identifier.isLValue()) { // push the value of the pointer
                        const size_t typeSize = idenType.getSizeBytes();
                        OUT << "popw BP\n"; // pop address back into BP
                        scope.pop(2);

                        for (size_t k = 0; k < typeSize; ++k)
                            OUT << "push [BP+" << k << "]\n";
                        scope.addPlaceholder(typeSize);
                    }
                } else { // handle primitives
                    // if this is an lvalue, pass the address
                    if (identifier.isLValue()) {
                        // push the address onto the stack
                        OUT << "movw BP, SP\n"; // move SP into BP for manipulation w/o affecting the SP
                        OUT << "sub BP, " << stackOffset << '\n';
                        OUT << "pushw BP\n";
                        scope.addPlaceholder(2);
                    } else { // this is an rvalue, push the value onto the stack
                        const size_t typeSize = idenType.getSizeBytes();
                        for (size_t j = 0; j < typeSize; ++j) {
                            OUT << "push [SP-" << stackOffset << "]\n";
                            scope.addPlaceholder();
                        }
                    }
                }
            } else {
                // push the address onto the stack
                OUT << "movw BP, SP\n"; // store the SP in BP for manipulation w/o affecting SP directly
                OUT << "sub BP, " << stackOffset << '\n';

                // if this is a reference pointer (ie. array passed as an argument), dereference it
                if (idenType.isReferencePointer()) {
                    // doesn't need to be a reference pointer anymore
                    idenType.setIsReferencePointer(false);

                    // push the referenced value
                    const size_t typeSize = idenType.getSizeBytes();
                    for (size_t k = 0; k < typeSize; ++k)
                        OUT << "push [BP+" << k << "]\n";
                    scope.addPlaceholder(typeSize);
                } else {
                    // not a reference pointer, push the address
                    OUT << "pushw BP\n"; // push the address of this identifier
                    scope.addPlaceholder(2);
                }
            }

            // update result type
            resultType = idenType;
            break;
        }
        case ASTNodeType::FUNCTION_CALL: {
            // lookup the function
            ASTFunctionCall& func = *static_cast<ASTFunctionCall*>(&bodyNode);
            const std::string& funcName = func.raw;
            if (labelMap.count(funcName) == 0) throw TUnknownIdentifierException(func.err);

            // find function and verify parameters match
            AssembledFunc* pDestFunc = nullptr;
            const size_t numParams = func.size();
            auto [itr, end] = labelMap.equal_range(funcName);
            for ((void)itr; itr != end; ++itr) {
                // check if parameters match
                const std::vector<Type>& paramTypes = itr->second.getParamTypes();
                if (paramTypes.size() != func.size()) continue;

                size_t j;
                for (j = 0; j < numParams; ++j) {
                    const Type& actualType = static_cast<ASTTypedNode*>(func.at(j))->getTypeRef();
                    if (paramTypes[j].isParamMatch(actualType, func.at(j)->err) == TYPE_PARAM_MISMATCH) break;
                }

                // if broken prematurely, a type didn't match
                if (j == numParams) {
                    pDestFunc = &itr->second;
                    break;
                }
            }

            if (pDestFunc == nullptr)
                throw TUnknownIdentifierException(bodyNode.err);

            // call the function
            OUT << "call " << pDestFunc->getStartLabel() << '\n';

            // pop args off stack after
            size_t paramTotalSize = 0;
            for (size_t j = 0; j < numParams; ++j) {
                paramTotalSize += resultTypes[j].getSizeBytes();
            }
            if (paramTotalSize > 0) {
                OUT << "sub SP, " << paramTotalSize << '\n';
                scope.pop(paramTotalSize);
            }
            resultType = pDestFunc->getReturnType();
            break;
        }
        case ASTNodeType::EXPR:
        case ASTNodeType::ARR_SUBSCRIPT: { // passthrough
            resultType = resultTypes[0];
            break;
        }
        case ASTNodeType::LIT_ARR: { // pass through
            resultType = static_cast<ASTArrayLiteral*>(&bodyNode)->getTypeRef();
            break;
        }
        case ASTNodeType::LIT_STRING: {
            // add to data section
            ASTStringLiteral* pStrLit = static_cast<ASTStringLiteral*>(&bodyNode);
            dataElements.push_back(DataElem(pStrLit->raw, DATA_TYPE_STRZ));
            resultType = pStrLit->getTypeRef();

            // write label
            OUT << "pushw " << STR_DATA_LABEL_PREFIX << nextStringDataID++ << '\n';
            scope.addPlaceholder(2);
            break;
        }
        case ASTNodeType::ASM: {
            // paste inline assembly to file
            ASTInlineASM* pASM = static_cast<ASTInlineASM*>(&bodyNode);
            OUT << pASM->getRaw() << '\n';
            resultType = pASM->getTypeRef();
            break;
        }
        case ASTNodeType::ASM_INST: {
            // handle the custom assembly instruction
            ASMProtectedInstruction* pInst = static_cast<ASMProtectedInstruction*>(&bodyNode);
            const TokenType instType = pInst->getInstType();

            switch (instType) {
                case TokenType::ASM_LOAD_AX:
                case TokenType::ASM_LOAD_BX:
                case TokenType::ASM_LOAD_CX:
                case TokenType::ASM_LOAD_DX: {
                    const size_t resultSize = resultTypes[0].getSizeBytes();
                    if (resultSize > 2 || resultSize == 0)
                        throw TInvalidOperationException(pInst->err);

                    // pad bytes
                    if (resultSize == 1) {
                        OUT << "push 0\n";
                        scope.addPlaceholder(1);
                    }

                    // move value into register
                    const std::string reg = instType == TokenType::ASM_LOAD_AX ? "AX" :
                                            instType == TokenType::ASM_LOAD_BX ? "BX" :
                                            instType == TokenType::ASM_LOAD_CX ? "CX" : "DX";

                    OUT << "popw " << reg << '\n';
                    scope.pop(2);
                    break;
                }
                case TokenType::ASM_READ_AX:
                case TokenType::ASM_READ_BX:
                case TokenType::ASM_READ_CX:
                case TokenType::ASM_READ_DX: {
                    // push the register value to the stack
                    const std::string reg = instType == TokenType::ASM_READ_AX ? "AX" :
                                            instType == TokenType::ASM_READ_BX ? "BX" :
                                            instType == TokenType::ASM_READ_CX ? "CX" : "DX";

                    OUT << "pushw " << reg << '\n';
                    scope.addPlaceholder(2);
                    break;
                }
                default: throw TSyntaxException(pInst->err);
            }

            resultType = pInst->getTypeRef();
            break;
        }
        case ASTNodeType::LIT_VOID: throw TIllegalVoidUseException(bodyNode.err);
        default: throw TExpressionEvalException(bodyNode.err);
    }

    // handle any subscripts
    ASTTypedNode* pTypedBody = dynamic_cast<ASTTypedNode*>(&bodyNode);
    if (pTypedBody != nullptr && pTypedBody->getNumSubscripts() > 0) {
        // grab address off stack
        if (!resultType.isPointer())
            throw TInvalidOperationException(bodyNode.err);

        const size_t numPointers = resultType.getNumPointers();
        const size_t numSubscripts = pTypedBody->getNumSubscripts();
        const std::vector<ASTArraySubscript*>& subscripts = pTypedBody->getSubscripts();
        const bool isLValue = pTypedBody->isLValue();

        if (numSubscripts > numPointers)
            throw TInvalidOperationException(bodyNode.err);

        // handle subscripts
        for (size_t j = 0; j < numSubscripts; ++j) {
            const size_t lastPtrSize = resultType.getPointers().back();
            const bool isImplicitArrayHint = resultType.getNumArrayHints() > 0 && lastPtrSize == TYPE_EMPTY_PTR;
            ASTArraySubscript* pSub = subscripts[j];
            resultType.popPointer();

            // determine the size of the rest of the object
            size_t chunkSize = resultType.getSizeBytes();

            // if this is a pointer (not an array), dereference and get its address
            if (lastPtrSize == TYPE_EMPTY_PTR && !isImplicitArrayHint) {
                // pop address into BP
                OUT << "popw BP\n";
                OUT << "push [BP+0]\n";
                OUT << "push [BP+1]\n";
            }

            // assemble subscript (ast_nodes.cpp makes sure these are all implicitly converted to int)
            assembleExpression(*pSub, outHandle, scope);

            OUT << "popw AX\n"; // pop subscript off stack
            OUT << "popw CX\n"; // pop address back into CX

            // if the chunk size isn't 1, scale subscript in AX by it
            if (chunkSize > 1) {
                OUT << "movw BX, " << chunkSize << '\n'; // move chunkSize into BX to force 16-bit
                OUT << "mul BX\n"; // scale by chunk size
            }
            OUT << (resultType.isUnsigned() ? "add" : "sadd") << " CX, AX\n"; // add the chunk to the pointer
            OUT << "pushw CX\n"; // put address back onto stack
            scope.pop(2); // 4 pops - 2 pushes = 2 net pops
        }

        // if this isn't an array and isn't an lvalue, dereference the final address on the stack
        if (!isLValue && (resultType.getNumPointers() == 0 || resultType.getPointers().back() == TYPE_EMPTY_PTR)) {
            // dereference final address on stack
            OUT << "popw BP\n"; // move address back into BP
            scope.pop(2);

            // push the referenced value
            const size_t typeSize = resultType.getSizeBytes();
            for (size_t k = 0; k < typeSize; ++k)
                OUT << "push [BP+" << k << "]\n";
            scope.addPlaceholder(typeSize);
        }
    }

    // implicit cast
    if (resultType != desiredType) {
        implicitCast(outHandle, resultType, desiredType, scope, bodyNode.err);
        resultType = desiredType; // update type to match
    }

    // mark this node as assembled
    bodyNode.isAssembled = true;

    // result type now matches desired type
    return resultType;
}

// implicitly converts a value pushed to the top of the stack to the given type
void implicitCast(std::ofstream& outHandle, Type resultType, Type desiredType, Scope& scope, const ErrInfo err) {
    if (resultType.isVoidNonPtr() && !desiredType.isVoidNonPtr()) throw TIllegalVoidUseException(err);

    // most importantly, if the two types are equal just return
    if (resultType == desiredType) return;

    // if the desired type is void, just pop everything off
    if (desiredType.isVoidNonPtr()) {
        size_t resultSize = resultType.getSizeBytes();
        if (resultSize > 0) {
            OUT << "sub SP, " << resultSize << '\n';
            scope.pop(resultSize);
        }
        return;
    }

    // if both pointers, just pass through (the address doesn't change)
    if (resultType.isPointer() && desiredType.isPointer()) return;

    // if converting between unsigned and signed of same primitive, pass through
    TokenType primA = resultType.getPrimType();
    TokenType primB = desiredType.getPrimType();
    if (primA == primB && !resultType.isPointer() && !desiredType.isPointer() &&
        resultType.isUnsigned() != desiredType.isUnsigned()) return;

    // if converting from a pointer of some sort to an integral type
    // pass through since they share the same size & everything
    bool wasTypePointer = false;
    if (!desiredType.isPointer() && resultType.isPointer() &&
        desiredType.getPrimType() != TokenType::TYPE_FLOAT &&
        desiredType.getPrimType() != TokenType::VOID) {
        resultType = MEM_ADDR_TYPE;
        primA = resultType.getPrimType();
        wasTypePointer = true;
    }

    // if converting from an integral type to a pointer,
    // pass through since they share the same size & everything
    if (!resultType.isPointer() && desiredType.isPointer() &&
        resultType.getPrimType() != TokenType::TYPE_FLOAT &&
        resultType.getPrimType() != TokenType::VOID) {
        desiredType = MEM_ADDR_TYPE;
        primB = desiredType.getPrimType();
        wasTypePointer = true;
    }

    // if converting between primitive types, pad/pop bytes
    if ((primA != primB || resultType.isUnsigned() != desiredType.isUnsigned() || wasTypePointer) && !resultType.isPointer() && !desiredType.isPointer()) {
        // handle cast to float vs other integral types
        if (primB == TokenType::TYPE_FLOAT) {
            throw TDevException("Float implicit casting not yet implemented!");
        } else if (primA == TokenType::TYPE_FLOAT) {
            throw TDevException("Float implicit casting not yet implemented!");
        } else {
            // converting between two integral types (just pad/pop bytes)
            const size_t startSize = getSizeOfType(primA);
            const size_t endSize = getSizeOfType(primB);

            if (primB == TokenType::TYPE_BOOL) {
                // if casting to a bool, enforce 1 or 0
                const std::string regA = startSize == 2 ? "AX" : "AL";
                OUT << (startSize == 2 ? "popw AX" : "pop AL") << '\n';

                // buffer value
                OUT << "buf " << regA << '\n';

                // if non-zero, set to 1
                const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                OUT << "jz " << mergeLabel << '\n';
                OUT << (startSize == 2 ? "movw AX" : "mov AL") << ", 1\n";
                OUT << "jmp " << mergeLabel << '\n';
                OUT << mergeLabel << ":\n";
                OUT << (startSize == 2 ? "pushw AX" : "push AL") << '\n';
            }

            // preserve sign bit in AL
            if (!resultType.isUnsigned()) {
                OUT << "mov AL, [SP-1]\n";
                OUT << "and AL, 0x80\n"; // get sign bit
            }

            if (startSize < endSize) { // pad bytes
                // if signed and negative, push 0xFFFFs, otherwise push 0s
                OUT << "movw CX, 0\n";
                if (!resultType.isUnsigned()) {
                    const std::string mergeLabel = JMP_LABEL_PREFIX + std::to_string(nextJMPLabelID++);
                    OUT << "buf AL\n";
                    OUT << "jz " << mergeLabel << '\n';
                    OUT << "movw CX, 0xFFFF\n"; // set CX to 1
                    OUT << "jmp " << mergeLabel << '\n';
                    OUT << mergeLabel << ":\n";
                }

                // add padding bytes
                for (size_t i = startSize; i < endSize; ++i) {
                    if (i+1 < endSize) {
                        OUT << "pushw CX\n";
                        ++i;
                    } else {
                        OUT << "push CL\n";
                    }
                }
                scope.addPlaceholder(endSize - startSize);
            } else if (startSize > endSize) { // pop bytes
                OUT << "sub SP, " << (startSize - endSize) << '\n';
                scope.pop(startSize - endSize);
            }

            // re-add sign bit
            if (!resultType.isUnsigned()) {
                OUT << "pop BL\n";
                OUT << "or BL, AL\n";
                OUT << "push BL\n";
            }
        }

        return;
    }

    // base case, illegal cast
    throw TIllegalImplicitCastException(err);
}