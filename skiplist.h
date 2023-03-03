#include<iostream>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>

using namespace std;

#define STORE_FILE "store/dumpFile"
mutex mtx,mtx1;
string delimiter = ":";

//声明Node节点
template<typename K,typename V>
class Node{
public:
    //构造函数
    Node(){}
    Node(K k,V v,int);
    //析构函数
    ~Node();
    //对键值对操作
    K get_key() const;
    V get_value() const;
    //设置V的值
    void set_value(V);
    //定义数组
    Node<K,V> **forward;
    //节点数
    int node_level;
private:
    K key;
    V value;
};
//Node有参构造
template<typename K,typename V>
Node<K,V>::Node(const K k,const V v,int level){
    this->key=k;
    this->value=v;
    this->node_level=level;
     // forward 大小为 level + 1
    this->forward=new Node<K,V>*[level+1];//分配内存
    memset(this->forward,0,sizeof(Node<K,V>*)*(level+1));
}
//Node析构函数
template<typename K,typename V>
Node<K,V>::~Node(){
    delete []forward;//释放内存
}
//Node键值对操作函数
template<typename K,typename V>
K Node<K,V>::get_key() const{
    return key;
}
template<typename K,typename V>
V Node<K,V>::get_value() const{
    return value;
}
template<typename K,typename V>
void Node<K,V>::set_value(V value){
    this->value=value;
}

//声明SkipList
template<typename K,typename V>
class SkipList{

public:
    //构造函数
    SkipList(int);
    //析构函数
    ~SkipList();
    Node<K,V>* create_node(K,V,int);
     // 增删改查操作函数
    int insert_element(K, V);   	 // 增
    int delete_element(K);	    	 // 删
    int update_element(K, V, bool);  // 改
    bool search_element(K);			 // 查
    void display_list();  // 打印跳表
    void clear(); 		  // 清空跳表
    int size();			  // 返回跳表节点数(不包含头节点)       
    // 数据落盘和数据加载
    void dump_file();
    void load_file();
    // 获得随机层高函数
    int get_random_level(); 

private:
    // 数据加载相关函数, 用来区分 key 和 value
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value);
    bool is_valid_string(const std::string& str);
private:
    int _max_level;  	   // 跳表层数上限
    int _skip_list_level;  // 当前跳表的最高层 
	int _element_count;    // 跳表中节点数
    Node<K,V> *_header;
    //文件操作符
    std::ofstream _file_writer;
    std::ifstream _file_reader; 
};
//构造函数
template<typename K,typename V>
SkipList<K,V>::SkipList(int max_level){
    this->_max_level=max_level;
    this->_skip_list_level=0;
    this->_element_count=0;
    K k;V v;
    this->_header=new Node<K,V>(k,v,max_level);
}
//析构函数
template<typename K,typename V>
SkipList<K,V>::~SkipList(){
    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    delete _header;
}

template<typename K,typename V>
Node<K,V>* SkipList<K,V>::create_node(const K k,const V v,int level){
    Node<K,V> *n=new Node<K,V>(k,v,level);
    return n;
}


template<typename K,typename V>
int SkipList<K,V>::get_random_level(){
    int k=1;
    while(rand()%2) k++;
    k=min(k,_max_level);
    return k;
}
template<typename K,typename V>
int SkipList<K,V>::size(){
    return _element_count;
}
//跳表插入节点函数实现部分
template<typename K,typename V>
int SkipList<K,V>::insert_element(const K key,const V value){
    //写操作，枷锁
    mtx.lock();
    //创建一个指针指向头节点
    Node<K,V>* current=this->_header;
    //创建一个update指针数组，跟踪位置
    Node<K,V> *update[_max_level+1];
    //清空update数组
    memset(update,0,sizeof(Node<K,V>*)*(_max_level+1));
    //从最高层开始遍历往下找
    for(int i=_skip_list_level;i>=0;i--){
        while(current->forward[i]!=NULL&&(current->forward[i]->get_key()<key)){
            current=current->forward[i];
        }
        update[i]=current;
    }
    /// 注意此时 current->get_key() >= key !!!
    current=current->forward[0];
    // 1. 插入元素已经存在
    if(current!=NULL&&current->get_key()==key){
        std::cout << "key: " << key << ", exists" << std::endl;
        mtx.unlock();
        return -1;  // 插入元素已经存在，返回 -1，插入失败
    }
    if(current==NULL||current->get_key()!=key){
        // 随机生成层的高度，也即 forward[] 大小
        int random_level = get_random_level();
        // 如果新添加的节点层高大于当前跳表层高，则需要更新 update 数组
        // 将原本[_skip_list_level random_level]范围内的NULL改为_header
        if(random_level>_skip_list_level){
            for(int i=_skip_list_level+1;i<=random_level;i++){
                update[i]=_header;
            }
            _skip_list_level=random_level;
        }
        // 创建节点，并进行插入操作
        Node<K,V>* inserted_node = create_node(key,value,random_level);
        for(int i=0;i<=random_level;i++){
            inserted_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = inserted_node;
        }
        std::cout << "Successfully inserted key: " << key << ", value: " << value << std::endl;
        _element_count ++;  // 更新节点数
    }
    mtx.unlock();
    return 0;
}
//跳表删除节点函数实现部分
template<typename K,typename V>
int SkipList<K,V>::delete_element(K key){
    // 笔者修改了其返回值，如果返回 0 删除成功，返回 -1 删除失败
    // 操作同 insert_element
    mtx.lock();
    //定义与之前插入的定义一致
    Node<K,V> *current=this->_header;
    Node<K,V> *update[_max_level+1];
    memset(update,0,sizeof(Node<K,V>*)*(_max_level+1));

    for(int i=_skip_list_level;i>=0;i--){
        while(current->forward[i]!=NULL&&current->forward[i]->get_key()<key){
            current=current->forward[i];
        }
        update[i]=current;
    }
    current=current->forward[0];
    if(current!=NULL&&current->get_key()==key){
         // 从最底层开始删除 update->forward 指向的节点，即目标节点
        for(int i=0;i<=_skip_list_level;i++){
            // 如果 update[i] 已经不指向 current 说明 i 的上层也不会指向 current
            // 也说明了被删除节点层高 i - 1。直接退出循环即可
            if(update[i]->forward[i]!=current)
                break;
             // 删除操作，等价于 node->next = node->next->next
            update[i]->forward[i]=current->forward[i];
        }
         // 因为可能删除的元素它的层数恰好是当前跳跃表的最大层数
        // 所以此时需要重新确定 _skip_list_level,通过头节点判断
        while(_skip_list_level > 0 && _header->forward[_skip_list_level] == 0){
            _skip_list_level--;
        }
        std::cout << "Successfully deleted key : "<< key << std::endl;
        _element_count --;
        mtx.unlock();
    	return 0; // 返回值 0 说明成功删除
    }else{
        std::cout << key << " is not exist, please check your input !\n";
        mtx.unlock();
        return -1; // 返回值 -1 说明没有该键值
    }
}
//跳表更新节点函数实现部分
template<typename K,typename V>
int SkipList<K,V>::update_element(const K key,const V value,bool flag=false){
    mtx1.lock();//插入操作,枷锁
    Node<K,V> *current=this->_header;
    Node<K,V> *update[_max_level+1];
    memset(update,0,sizeof(Node<K,V>*)*(_max_level+1));

    for(int i=_skip_list_level;i>=0;i--){
        while(current->forward[i]!=NULL&&current->forward[i]->get_key()<key){
            current=current->forward[i];
        }
        update[i]=current;
    }
    current=current->forward[0];
     // 1. 插入元素已经存在
     if(current!=NULL&&current->get_key()==key){
        std::cout << "key: " << key << ", exists" << std::endl;
        std::cout << "old value : " << current->get_value() << " --> "; // ~ 打印 old value
        current->set_value(value);  // 重新设置 value, 并打印输出。
        std::cout << "new value : " << current->get_value() << std::endl;
        mtx1.unlock();
        return 1;  // 插入元素已经存在，只是修改操作，返回 1 说明更新成功
     }
     // 2. 插入元素不存在
     if(flag){
        SkipList<K, V>::insert_element(key, value);
        mtx1.unlock();
        return 0;  // 说明 key 不存在，但是创建了它
     } else {
        std::cout << key << " is not exist, please check your input !\n";
        mtx1.unlock();
        return -1; // 表示 key 不存在，并且不被允许创建
    } 
    
}
//跳表搜索节点函数实现部分
template<typename K, typename V> 
bool SkipList<K, V>::search_element(K key) {
    std::cout << "search_element..." << std::endl;
    Node<K, V> *current = _header;
    // 从最高层开始遍历，找到最底层中最后一个满足小于key的节点
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }
    current = current->forward[0]; // 该操作后 current->get_key >= key 或者 null
    // 找到
    if (current != NULL && current->get_key() == key) {
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }
	// 没找到
    std::cout << "Not Found Key: " << key << std::endl;
    return false;
}
template<typename K, typename V> 
void SkipList<K, V>::display_list() {
    std::cout << "\n******** Skip List ********"<<"\n"; 
    // 逐层打印
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i]; 
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}
// Dump data in memory to file 
template<typename K, typename V> 
void SkipList<K, V>::dump_file() {
    std::cout << "dump_file..." << std::endl;
    _file_writer.open(STORE_FILE);  // 打开文件，写操作
    Node<K, V> *node = this->_header->forward[0]; 
    // 只写入键值对，放弃层信息
    while (node != NULL) {
        // 文件写入（key value 以 : 为分隔符），及信息打印
        _file_writer << node->get_key() << ":" << node->get_value() << "\n"; 
        std::cout << node->get_key() << ":" << node->get_value() << ";\n";   
        node = node->forward[0];
    }
    _file_writer.flush();
    _file_writer.close();
    return ;
}

// Load data from disk
template<typename K, typename V> 
void SkipList<K, V>::load_file() {
    _file_reader.open(STORE_FILE);  // 打开文件，读操作
    std::cout << "load_file..." << std::endl;
    std::string line;
    // key 与 value 是一个指向 string 对象的指针
    std::string* key = new std::string();
    std::string* value = new std::string();
    while (getline(_file_reader, line)) { // 一行一行写入
        get_key_value_from_string(line, key, value);  // 辅助函数
        if (key->empty() || value->empty()) {
            continue;
        }
        // 重新载入过程使用 insert_element()
        // 所以层之间的关系(各节点的层高)可能发生变化, 所以与之前的SkipList不同
        insert_element(*key, *value);  
        std::cout << "key:" << *key << "value:" << *value << std::endl;
    }
    _file_reader.close();
}
// 辅助函数
template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string& str, std::string* key, std::string* value) {
    if(!is_valid_string(str)) {
        return;
    }
    // 分隔符之前的为 key, 分隔符之后的为 value
    *key = str.substr(0, str.find(delimiter));  
    *value = str.substr(str.find(delimiter) + 1, str.length()); 
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    // 没有发现分隔符
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}
//跳表清理函数实现部分
template<typename K, typename V> 
void SkipList<K, V>::clear() { 
    std::cout << "clear ..." << std::endl;
    Node<K, V> *node = this->_header->forward[0]; 
    // 删除节点
    while (node != NULL) {
        Node<K, V> *temp = node;
        node = node->forward[0];
        delete temp;
    }
    // 重新初始化 _header
    for (int i = 0; i <= _max_level; i++) {
        this->_header->forward[i] = 0;
    }
    this->_skip_list_level = 0;
    this->_element_count = 0;
}