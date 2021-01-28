<img src="https://i.loli.net/2020/12/01/DdoN7p5VC6lHjsK.png" alt="image-20201201150729333" style="zoom: 50%;" />

## 抽象语法（AST）

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201152708246.png" alt="image-20201201152708246" style="zoom: 80%;" />

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201153045728.png" style="zoom:90%;" />

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201153142420.png" alt="image-20201201153142420" style="zoom:92%;" />

## 语义分析（Type checking）

阶段任务：

- 将Identifier映射到它们的类型和存储位置（利用symbol table）
- 将抽象语法转化成更简单、适合于生成机器代码的表示

代码结构：

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201170631364.png" alt="image-20201201170631364" style="zoom:80%;" />

- semant：
- translate：
- frame：
- temp：

### symbol

为了避免字符串比较，把每个字符串转化为一个`symbol`对象，它的所有出现都被映射成了同一个symbol

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201154033135.png" alt="image-20201201154033135" style="zoom: 70%;" />

- `S_table` : 将 `S_Symbols` 映射到 bindings
-  `S_Symbol` 和  `S_name` 在symbol 和 name间相互转换
-  S_empty 创建一个新的表
-  S_enter 插入绑定 $x\to b$, x是一个symbol指针
- `S_beginScope` remembers the current state of the table 
- `S_endScope` restores the table to where it was at the most recent `S_beginScope`  that has not already been ended

### type

绑定的是什么？类型标识符关联`Ty_ty`

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201155056429.png" alt="image-20201201155056429" style="zoom:67%;" />

- 基本类型是`int`和`string`

- 复合类型有`record`、`array`

  - `record`记录每个域的名字和类型（`Ty_fieldList`）
  - `array`记录数组类型

  它们都记录了变量的地址，因此两个看似相同的并不是同一种类型

- `nil`属于任何类型

- `void`表示没有返回值的表达式

- `name`用来占位那些只知道名字不知道定义的类型，用`Ty_Name(sym, NULL)`来创造类型名是`sym`的类型，稍后填充`ty`域。

### env

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201190522424.png" alt="image-20201201190522424" style="zoom: 67%;" />

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201190715629.png" alt="image-20201201190715629" style="zoom:67%;" />

- 对于值标识符我们需要知道它是变量(`E_varEntry`)或者函数(` E_funEntry`)，我们用`E_enventry` 来记录这个信息。

  - 变量需要知道类型(`Ty_ty ty`)是什么

  - 函数需要知道：

    - 参数(`Ty_tyList formals`)
    - 返回类型(` Ty_ty result`)

    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201160923952.png" alt="image-20201201160923952" style="zoom:80%;" />

    （`Ty_tyList` 是一连串的 type）

    - `Tr_level level`

    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201190544905.png" alt="image-20201201190544905" style="zoom: 67%;" />

- `base_tenv`:  maps the symbol `int` to `Ty_Int` and `string` to `Ty_String`
- `base_venv`:  contains bindings for predefined functions`flush`, `ord`, `chr`, `size`, and so on

### semant

执行语义分析（类型检查），*后续还会扩充功能：将表达式转化为中间代码*

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201162035605.png" alt="image-20201201162035605" style="zoom:80%;" />

代码的逻辑：调用语法分析器来生成 `A_exp`, 接着对这个表达式调用 `SEM_transProg`

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201161524564.png" alt="image-20201201161524564" style="zoom:80%;" />

- `trans*`分别转换 `A_var`、`A_exp` 和 `A_Dec` 三种表达式

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201161734227.png" alt="image-20201201161734227" style="zoom:80%;" />

- 返回值是 `expty`，包含两部分：
  - `Tr_exp exp`: 转换为中间代码的表达式
  - ` Ty_ty ty`：表达式的类型

## 语义分析（Activation Records）

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201163445549.png" alt="image-20201201163445549" style="zoom: 67%;" />

### frame/x86frame

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201163837336.png" alt="image-20201201163837336" style="zoom:80%;" />

+ `F_frame`：栈帧的抽象表示，有关形式参数和分配在栈帧中局部变量的信息。

  + F_accessList：the locations of all the formals
  + T_stm：instructions required to implement the “view shift”
  + int size: 需要分配的栈帧大小
  + Temp_label：and the label at which the function’s machine code is to begin

+ `F_access`：描述存放在栈中或寄存器中的形式参数和局部变量。

  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201164831893.png" alt="image-20201201164831893" style="zoom:80%;" />

  - 可以由 `InFrame()` 和 `InReg()` 构造 `F_access` 对象。

+ `F_newFrame(Temp_label name, U_boolList formals)`：返回一个新栈帧。

  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201164610709.png" alt="image-20201201164610709" style="zoom:80%;" />

+ `F_formals()`：返回形式参数的位置（从被调用函数的角度来看）。

+ `F_allocLocal(F_frame f, bool escape)`：分配一个新的变量。

  + 逃逸变量 `InFrame`
  + 非逃逸变量 `InReg`

```c
// 参考头文件
/* frame.h */
typedef struct F_frame_ *F_frame;
typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;
struct F_accessList_ {F_access head; F_accessList tail;};
F_accessList F_AccessList(F_access head, F_accessList tail);

typedef struct F_frag_ *F_frag;
struct F_frag_ {
enum {F_stringFrag, F_procFrag} kind;
union {struct {Temp_label label; string str;} stringg;
struct {T_stm body; F_frame frame;} proc;
} u;
};
F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ {F_frag head; F_fragList tail;};
F_fragList F_FragList(F_frag head, F_fragList tail);

Temp_map F_tempMap;
Temp_tempList F_registers(void);
string F_getlabel(F_frame frame);
T_exp F_Exp(F_access acc, T_exp framePtr);
F_access F_allocLocal(F_frame f, bool escape); //(see p. 139)
F_accessList F_formals(F_frame f); //(p. 137)
Temp_label F_name(F_frame f); ///(p. 136)
extern const int F_wordSize; //(p. 159)
Temp_temp F_FP(void); //(p. 159)
Temp_temp F_SP(void);
Temp_temp F_ZERO(void);
Temp_temp F_RA(void);
Temp_temp F_RV(void); //(p. 172)
F_frame F_newFrame(Temp_label name, U_boolList formals); //(p. 136)
T_exp F_externalCall(string s, T_expList args); //(p. 168)
F_frag F_string (Temp_label lab, string str); //(p. 269)
F_frag F_newProcFrag(T_stm body, F_frame frame);
T_stm F_procEntryExit1(F_frame frame, T_stm stm); //(p. 267)
AS_instrList F_procEntryExit2(AS_instrList body); //(p. 215)
AS_proc F_procEntryExit3(F_frame frame, AS_instrList body);

//c
T_stm F_procEntryExit1(F_frame frame, T_stm stm) {
	return stm;
}

static Temp_tempList returnSink = NULL;
AS_instrList F_procEntryExit2(AS_instrList body) {
    if (!returnSink) returnSink = Temp_TempList(ZERO, Temp_TempList(RA,
    Temp_TempList(SP, calleeSaves)));
    return AS_splice(body, AS_InstrList(AS_Oper("", NULL, returnSink, NULL), NULL));
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body) {
    char buf[100];
    sprintf(buf,"PROCEDURE %s\n", S_name(frame->name));
    return AS_Proc(String(buf), body, "END\n");
}
```



#### translation helper

frame模块还需要包括机器相关的定义（7.2.2简单变量）

- 接口

  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201204231650809.png" alt="image-20201204231650809" style="zoom:50%;" />

  - `F_FP`: 返回当前的fp
  
  - `F_Exp`：将一个`F_access`转换为一个Tree表达式（`T_exp`）
  
    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201204232243336.png" alt="image-20201204232243336" style="zoom: 60%;" />
  
    ​																						const k是offset
  
    运行时是在translate中被调用的
  
    ```c
    F_Exp(acc, T_Temp(F_FP()))
    ```
  
    传递 ` T_Temp(F_FP()` 的原因是，必须要获得当前access所在的frame pointer，如果access是一个Reg，就简单的忽略fp
  
- 调用运行时系统的函数
  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205141807398.png" alt="image-20201205141807398" style="zoom:50%;" />

  - Tiger没有对存储器进行管理的机制，所以需要调用外部c或汇编编写的函数

  - 实现：

    ```c
    T_exp F_externalCall(string s, T_expList args) { 
        return T_Call(T_Name(Temp_namedlabel(s)), args); 
    }
    ```

    用 `F_externalCall` 包裹是因为可能涉及一些额外的处理，例如下划线

  - 调用：

    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205142824838.png" alt="image-20201205142824838" style="zoom:50%;" />

#### fragment part

（7.3.3 fragment）

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205132712877.png" alt="image-20201205132712877" style="zoom:48%;" />

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205132842664.png" alt="image-20201205132842664" style="zoom:50%;" />



### escape

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201165655315.png" alt="image-20201201165655315" style="zoom:75%;" />

- `Esc_findEscape(A_exp exp)`
  - 遍历语法树（和类型检查相似），判断每一个变量
  - `Esc_findEscape`需要在semantic分析之前
  - 每当发现变量或函数参数声明时，如A_VarDec(name=symbol("a"), escape=r, ...), 调用EscapeEntry
- `EscapeEntry(d, &(x->escape))`: 引入一个新的表，在d深度绑定一个symbol。将x的escape设为false。在大于d的深度使用时，设为true

### temp

​	语义分析阶段还不知道变量具体要保存在哪里。所以用temp表示暂时保存在寄存器中的这些变量。用label表示其准确地址还需要确定。

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201170429557.png" alt="image-20201201170429557" style="zoom:80%;" />

- `Temp_temp`：临时变量，局部变量的抽象名，暂时保存在寄存器中的值。
- `Temp_label`：标号，静态存储器地址的抽象名，机器语言中的位置。
- `Temp_newtemp()` returns a new temporary from an infinite set of temps. 
- `Temp_newlabel()` returns a new label from an infinite set of labels.
- `Temp_namedlabel(string)` returns a new label whose assembly-language name is string

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201215193550240.png" alt="image-20201215193550240" style="zoom:70%;" />

### translate

Translate 模块为 semant 模块管理着局部变量和静态函数嵌套。

```c
typedef struct Tr_access_ *Tr_access;
struct Tr_access_ {
	Tr_level level;
	F_access access;
};

typedef struct Tr_level_ *Tr_level;
struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
    Tr_accessList formals; // frame翻译过来的
};

typedef struct Tr_accessList_ *Tr_accessList;
struct Tr_accessList_ {
	Tr_access head;
	Tr_accessList tail;	
};
```

+ `Tr_access`：比 `F_access` 多知道一个与静态链相关的信息 (`Tr_level`)
+ `Tr_level`：比 `F_frame` 多知道在第几层（父节点 `Tr_level parent`）

```c
Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail);
Tr_level Tr_outermost(void); // 返回相同的值，代表包含main函数的层次
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);
```

+ `Tr_newLevel()`：创建新的嵌套层，调用 `F_newFrame()`
+ `Tr_formals()`: 调用`F_formals()`, 并转化为 `Tr_access`
+ `Tr_allocLocal()`：在指定的层次中创建变量，调用 `F_allocLocal()`

管理静态链、追踪层次信息

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201201164027897.png" alt="image-20201201164027897" style="zoom:80%;" />



## 翻译中间代码

中间表示中的个体成分应该表示很简单的事，以方便用机器指令来表示。

### tree

IR的数据结构T_exp和T_stm

```c
typedef struct T_stm_ *T_stm;
typedef struct T_exp_ *T_exp;
```

### translate

#### 结构和辅助


- 创建一种抽象类型（`Tr_exp`）模拟三种表达式：`Ex`, `Nx`, `Cx`：
  - `Ex`：`T_exp`
  - `Nx`：`T_stm`
  - `Cx`：一个结构体
  ```c
  typedef struct Tr_exp_ *Tr_exp;
  struct Tr_exp_ {
      enum {Tr_ex, Tr_nx, Tr_cx} kind;
      union {T_exp ex; T_stm nx; struct Cx cx; } u;
  };
  
  typedef struct Tr_expList_ *Tr_expList;
  struct Tr_expList_ {
      Tr_exp head;
      Tr_expList tail;
  };
  
  struct Cx {
  	patchList trues; patchList falses; 
  	T_stm stm; //真实的语句，比如 T_Cjump
  };
  ```
  
  
    - `patchList` :

  	```c
  	typedef struct patchList_ *patchList;
  	struct patchList_ {Temp_label *head; patchList tail;};
  	static patchList PatchList(Temp_label *head, patchList tail);
  	```
  - `doPatch(patchList tList, Temp_label label)` : 利用 `label` 回填标号回填表 `patchList` 中所有代填的标号。
  
    - `joinPatch(patchList first, patchList second)`: 
  
- 还需要三种表达式之间相互转换：

  ```c
  static T_exp unEx(Tr_exp e);
  static T_stm unNx(Tr_exp e);
  static struct Cx unCx(Tr_exp e);
  ```

  `unEx` 接受一个 `Tr_exp` 类型的参数，将其转化为 `T_exp` 类型

  `unNx`, `unCx` 同理，需要注意的是 `unCx` 应该拒绝来自 `Nx` 的转换
  
  

#### 实际翻译

Translate模块还需要实现所有MEM节点的管理，这样Semant模块才不需要依赖中间语言Tree的表示（7.2.2简单变量）


  - 简单变量（7.2.2）

    ```c
    Tr_Exp Tr_simpleVar(Tr_Access, Tr_Level);
    ```

    > Semant模块中的逻辑为
    >
    > 1. 在venv中查询到对应S_symbol的Tr_Access
    > 2. 当前的level就是x所在函数的level
    > 3. 传递x的access和所在函数的level给Tr_simpleVar

    ```c
    Tr_exp Tr_simpleVar(Tr_access access, Tr_level level) {
      T_exp fp = T_Temp(F_FP()); // 当前的fp
      while (level && level != access->level) {
        F_access sl = F_formals(level->frame)->head;
        fp = F_Exp(sl, fp); // 不停地把fp替换成新的IR tree（MEM（fp，offset））
        level = level->parent;
      }
      return Tr_Ex(F_Exp(access->access, fp));
    }
    ```

  - 条件表达式
    
  - 字符串（7.2.10）
    
    - 引入fragment（见frame模块）
    
      <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205132943648.png" alt="image-20201205132943648" style="zoom:50%;" />
    
    - 对于每个字符串文字常数 lit，Translate 模块做两个操作：
    
      + 生成一个新的标号 lab，并返回这个标号的树中间表示 `T_Name(lab)`
      
    ```c
        Temp_label l = Temp_newlabel();
        return Tr_Ex(T_Name(l));
    ```
    
      + 将片段 `F_String(lab, lit)` 放到一个全局表(fragList)中
      
        ```c
        F_frag f = F_StringFrag(l, buf);
        fragList = F_FragList(f, fragList);
        ```
    
    - 注意：buf不只是字符，还需要告诉编译器长度，具体实现可以是：
      <img src="file:///C:\Users\olivia\Documents\Tencent Files\758970771\Image\Group2\H~\7}\H~7}B@69ETIKHGOXJA[O[FR.jpg" alt="img" style="zoom:70%;" />
    
- record和array的创建（7.2.11）


  - array

  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205141725636.png" alt="image-20201205141725636" style="zoom: 40%;" />

  - record

    ```c
    Temp_temp r = Temp_newtemp();
    T_exp ex = T_Eseq(T_Seq(T_Move(T_Temp(r), 
    F_externalCall("malloc", T_ExpList(T_Const(n * F_wordSize), NULL))), 
    moveFields(r, n, fields)), T_Temp(r));
    return Tr_Ex(ex);
    ```

- loops（7.2.12/13）

  - while

    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205143418102.png" alt="image-20201205143418102" style="zoom:50%;" />

  - break: jump to done(label)

    ```c
    Tr_exp Tr_break(Temp_label done) {
        return Tr_Nx(T_Jump(T_Name(done), Temp_LabelList(done, NULL)));
    }
    ```

  - for

    <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201205144100951.png" alt="image-20201205144100951" style="zoom:50%;" />

## 基本块和轨迹

将任意一棵树转换成由 `T_stm` 组成的表的过程分为三步：

1. 将树重写为不含 SEQ 和 ESEQ 节点的规范树：Linearize
2. 将规范树分组组合成其内不含转移和标号的基本块集合：BasicBlocks
3. 对基本块排序并形成一组轨迹，轨迹中每一个 CJUMP 之后都直接跟随它的 false 标号：traceSchedule

### canon

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216001309635.png" alt="image-20201216001309635" style="zoom: 80%;" />

#### Linearize

```c
T_stmList C_linearize(T_stm stm){
    return linear(do_stm(stm), NULL);
}
static T_stm do_stm(T_stm stm); //得到一个SEQ集中在前面的tree
static T_stm reorder(expRefList rlist); //重写，将ESEQ和CALL放到最前面
```

采用规则
$$
ESEQ(s1, ESEQ(s2, e)) = ESEQ(SEQ(s1,s2), e)\\
BINOP(op, ESEQ(s, e1), e2) = ESEQ(s, BINOP(op, e1, e2))\\
MEM(ESEQ(s, e1)) = ESEQ(s, MEM(e1))\\
JUMP(ESEQ(s, e1)) = SEQ(s, JUMP(e1))\\
CJUMP(op, ESEQ(s, e1), e2,l1,l2) = SEQ(s, CJUMP(op, e1, e2,l1,l2))\\
BINOP(op, e1, ESEQ(s, e2)) = ESEQ(MOVE(TEMP t, e1),ESEQ(s, BINOP(op, TEMP t, e2)))\\
CJUMP(op, e1, ESEQ(s, e2),l1,l2) = SEQ(MOVE(TEMP t, e1),SEQ(s, CJUMP(op, TEMP t, e2,l1,l2)))\\
BINOP(op, e1, ESEQ(s, e2)) = ESEQ(s, BINOP(op, e1, e2))\\
CJUMP(op, e1, ESEQ(s, e2),l1,l2) = SEQ(s, CJUMP(op, e1, e2,l1,l2))
$$
采用规则$CALL(fun, args) → ESEQ(MOVE(TEMP\ t, CALL(fun, args)), TEMP\ t)$把CALL放到最前面

采用规则 $SEQ(SEQ(a, b), c) = SEQ(a,seq(b, c))$， 线性化(C_linearize)得到一个$SEQ(s1, SEQ(s2,..., SEQ(s_{n−1},s_n) . . .))$

#### BasicBlocks

#### traceSchedule

Basic的顺序可以任意选择，采用如下的算法：

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216002617510.png" alt="image-20201216002617510" style="zoom: 70%;" />

## 指令选择

将IR tree翻译成机器指令序列

### assem

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216153647621.png" alt="image-20201216153647621" style="zoom:80%;" />

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216153436962.png" alt="image-20201216153436962" style="zoom:80%;" />![image-20201216153520558](C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216153520558.png)

### codgen

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216150402605.png" alt="image-20201216150402605" style="zoom:80%;" />

- 参数：

  - F_frame f：需要将帧指针替换为栈指针，为了知道frame label，frame size是在分配完成之后知道的，由F_procEntryExit3添加
  - T_stmList: 将简化后的IR tree传入

- 在main中调用F_procEntryExit2在结尾添加活跃的寄存器

- 具体工作：

  ```c
  static AS_instrList iList=NULL, last=NULL;
  AS_instrList F_codegen(F_frame f, T_stmList stmList) {
    AS_instrList list;
    // 使用Maximal Munch算法实现IR树到Assem汇编指令的在转换
    for (T_stmList sl = stmList; sl; sl = sl->tail)
      munchStm(sl->head);
    // 返回汇编指令链表（AS_instrList）
    list = iList; iList = last = NULL;
    return list;
  }
  ```

- 实现munchStm

  <img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201216153540466.png" alt="image-20201216153540466" style="zoom:80%;" />

  ```c
  static void munchStm(T_stm s) {
    switch ( s )
      case EXP(CALL(e,args)):
   		Temp_temp r = munchExp(e);
          Temp_tempList l = munchArgs(0,args);
          emit(AS_Oper("CALL ‘s0\n", calldefs, L(r,l), NULL)); //将AS_Oper()这个instru加入全局iList中
  }
  ```

## 活跃分析

### flowgraph

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201210171204624.png" alt="image-20201210171204624" style="zoom:67%;" />

- `FG_def(n)`: 指令n定义的变量列表
- `FG_use(n)`: 指令n使用过的变量列表
- `FG_isMove(n)`：判断是否是move指令，后续可以决定是否可以删除

### liveness

<img src="C:\Users\olivia\AppData\Roaming\Typora\typora-user-images\image-20201210162806905.png" alt="image-20201210162806905" style="zoom:67%;" />

- Live_graph:
  - G_graph graph: 冲突图
  - Live_moveList: 节点偶对组成的表
