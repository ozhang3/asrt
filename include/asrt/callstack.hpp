#ifndef ADAE7552_4608_4C7D_BD8A_3E583DAFC2C5
#define ADAE7552_4608_4C7D_BD8A_3E583DAFC2C5


namespace Util{

/**
 * @brief A thread-local stack of user defined key-value markers
 * 
 * @tparam Key Establishes identify of a given marker
 * @tparam Value The value that the key is associated with
 */
template <typename Key, typename Value = unsigned char>
class CallStack{
public:
    /* A marker objects associates a key-value pair and places  
        it on the stack automatically upon construction */
    class Marker{
    public:
        /* push key to stack */
        explicit Marker(Key* key)
            : key_{key}, 
              value_{reinterpret_cast<unsigned char*>(this)},
              lower_{CallStack<Key, Value>::top_}
        {
            CallStack<Key, Value>::top_ = this;
        }

        /* push key-val pair to stack */
        Marker(Key* key, Value& value)
            : key_{key}, 
              value_{&value},
              lower_{CallStack<Key, Value>::top_}
        {
            CallStack<Key, Value>::top_ = this;
        }

        /* pop stack */
        ~Marker(){
            CallStack<Key, Value>::top_ = lower_;
        }
    private:
        friend class CallStack<Key, Value>;
        Key* key_;
        Value* value_;
        Marker* lower_; /* the elem below us in the stack */
    };

    friend class Marker;

    /* returns the ptr to Value that the given Key is associated with
        if key is marked to this specific CallStack; else returns nullptr */
    static Value* Contains(Key* key){
        Marker* elem{top_};
        while(elem){
            if(elem->key_ == key){
                return elem->value_;
            }
            elem = elem->lower_;
        }
        return nullptr;
    }

private:
    /* a thread local call stack marker to enable
        separate call stacks for distinct threads */
    static inline thread_local Marker* top_{nullptr};
};

}

#endif /* ADAE7552_4608_4C7D_BD8A_3E583DAFC2C5 */
