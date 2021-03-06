/**
 * A simple implementation of B+Tree ( the constant is quiet large ... )
 * Finished by Edersnow(github) in 2020/5/31
 * Go to README.md to see more:)))
 */
#include <functional>
#include <cstddef>
#include <cstring>
#include "exception.hpp"

namespace sjtu {
    template <class Key, class Value>
    class BTree {
    const static size_t node_size=4096;
    const static size_t max_number_of_children=(node_size-sizeof(size_t)-4*sizeof(long)-sizeof(bool))/(sizeof(Key)+sizeof(long));
    const static size_t max_number_of_data=(node_size-sizeof(size_t)-4*sizeof(long))/(sizeof(Key)+sizeof(Value));
    private:
        //the basic information of the bpt
        struct infor{
            long root_offset;
            long leaf_offset;
            bool rootisleaf;
            size_t file_length;
        };

        struct Divider{
            Key key;
            long childpointer;
        };

        struct Data{
            Key key;
            Value value;
        };

        struct internal_node{
            long offset;
            long prevnode;
            long nextnode;
            long parenode;
            bool childisleaf;
            size_t current_length;
            Divider divider[max_number_of_children];
            internal_node():offset(0), prevnode(0), nextnode(0), parenode(0), current_length(0), childisleaf(false){}
        };

        struct leaf_node{
            long offset;
            long prevnode;
            long nextnode;
            long parenode;
            size_t current_length;
            Data data[max_number_of_data];
            leaf_node():offset(0), prevnode(0), nextnode(0), parenode(0), current_length(0){}
        };

        class fail_in_file_IO{};

        FILE *current_file;
        infor basic_data;
        char file_name[100];

        inline void read_data(void *dat, size_t siz, long off){
            fseek(current_file, off, SEEK_SET);
            if(fread(dat, siz, 1, current_file)==0)  throw fail_in_file_IO();
        }

        inline void write_data(void *dat, size_t siz, long off){
            fseek(current_file, off, SEEK_SET);
            if(fwrite(dat, siz, 1, current_file)==0)  throw fail_in_file_IO();
        }

        inline void initialize(){
            leaf_node tmp;
            current_file=fopen(file_name, "wb+");
            if(!current_file)  throw fail_in_file_IO();
            tmp.offset=sizeof(infor);
            basic_data.root_offset=basic_data.leaf_offset=sizeof(infor);
            basic_data.file_length=sizeof(infor)+sizeof(leaf_node);
            basic_data.rootisleaf=true;
            write_data(&basic_data, sizeof(infor), 0);
            write_data(&tmp, sizeof(leaf_node), sizeof(infor));
        }

        template <class T>
        inline void update_parent(internal_node &curinter, size_t lef, size_t rig){
            T tmp;
            for(size_t i=lef;i<=rig;++i){
                read_data(&tmp, sizeof(T), curinter.divider[i].childpointer);
                tmp.parenode=curinter.offset;
                write_data(&tmp, sizeof(T), curinter.divider[i].childpointer);
            }
        }
        //***wrapper function** update parenode of the child of curinter from lef to rig
        inline void update_parent_wrapper(internal_node &curinter, size_t lef, size_t rig){
            if(curinter.childisleaf)  update_parent<leaf_node>(curinter, lef, rig);
            else  update_parent<internal_node>(curinter, lef, rig);
        }

        inline void update_divider(internal_node &curinter, const Key &oldkey, const Key &newkey){
            size_t mid=find_divider(curinter, oldkey);
            curinter.divider[mid].key=newkey;
            if(mid==0)  update_divider_wrapper<internal_node>(curinter, oldkey, newkey);
        }
        template <class T>
        inline void update_divider_wrapper(T &curnode, const Key &oldkey ,const Key &newkey){
            if(curnode.parenode){
                internal_node tmp;
                read_data(&tmp, sizeof(internal_node), curnode.parenode);
                update_divider(tmp, oldkey, newkey);
                write_data(&tmp, sizeof(internal_node), curnode.parenode);
            }
        }

        /***    the following functions are related to search    ***/

        //find the **possible** leafnode (**if the key is the smallest, return the first node**)
        inline long findleaf(const Key &key){
            internal_node poi;
            size_t lef, rig, mid;
            long tmp=basic_data.root_offset;
            if(!basic_data.rootisleaf){
                do{
                    read_data(&poi, sizeof(internal_node), tmp);
                    lef=0, rig=poi.current_length-1;
                    while(lef!=rig){
                        mid=lef+rig+1>>1;
                        if(poi.divider[mid].key<=key)  lef=mid;
                        else  rig=mid-1;
                    }
                    tmp=poi.divider[lef].childpointer;
                }while(!poi.childisleaf);
            }
            return tmp;
        }

        //*** if all the data.key is smaller than key, return current_length **
        inline size_t find_lowbound(leaf_node &curleaf, const Key &key){
            size_t lef=0, rig=curleaf.current_length-1, mid;
            if(curleaf.current_length==0)  return lef;  //in case that the root is leaf
            while(lef!=rig){
                mid=lef+rig>>1;
                if(curleaf.data[mid].key>=key)  rig=mid;
                else  lef=mid+1;
            }
            if(curleaf.data[lef].key<key)  lef=curleaf.current_length;
            return lef;
        }

        //*** no exception ** 
        inline size_t find_divider(internal_node &curinter, const Key &key){
            size_t lef=0, rig=curinter.current_length-1, mid;
            while(true){
                mid=lef+rig>>1;
                if(curinter.divider[mid].key==key)  break;
                if(curinter.divider[mid].key>key)  rig=mid-1;
                else  lef=mid+1;
            }
            return mid;
        }

        /***    the following functions are related to insertion    ***/

        inline void create_parent(internal_node &curinter, Key &childkey, long childaddress){
            basic_data.root_offset=curinter.offset=basic_data.file_length, basic_data.file_length+=sizeof(internal_node);
            curinter.divider[0].key=childkey;
            curinter.divider[curinter.current_length++].childpointer=childaddress;
        }

        //add divider in curinter (newkey>oldkey)
        //*** no exception **
        inline void add_divider(internal_node &curinter, Key &oldkey, Key &newkey, long oldaddress, long newaddress){
            size_t mid=find_divider(curinter, oldkey);
            for(size_t i=curinter.current_length++;i>mid+1;--i)  curinter.divider[i]=curinter.divider[i-1];
            curinter.divider[mid+1].childpointer=newaddress;
            curinter.divider[mid+1].key=newkey;
            curinter.divider[mid].childpointer=oldaddress;
        }

        //the curinter change to which the key locates in
        void split_internal_node(internal_node &curinter, const Key &key){
            internal_node splinter, curparent;
            if(curinter.parenode){
                read_data(&curparent, sizeof(internal_node), curinter.parenode);
                if(curparent.current_length==max_number_of_children)
                    split_internal_node(curparent, curinter.divider[0].key);
            }
            else  create_parent(curparent, curinter.divider[0].key, curinter.offset);

            splinter.offset=basic_data.file_length, basic_data.file_length+=sizeof(internal_node);
            splinter.prevnode=curinter.prevnode;
            splinter.nextnode=curinter.offset;
            splinter.parenode=curparent.offset;
            splinter.childisleaf=curinter.childisleaf;
            splinter.current_length=max_number_of_children>>1;
            for(size_t i=0;i<splinter.current_length;++i)  splinter.divider[i]=curinter.divider[i];
            update_parent_wrapper(splinter, 0, splinter.current_length-1);
            if(splinter.prevnode){
                internal_node tmp;
                read_data(&tmp, sizeof(internal_node), splinter.prevnode);
                tmp.nextnode=splinter.offset;
                write_data(&tmp, sizeof(internal_node), splinter.prevnode);
            }

            curinter.parenode=curparent.offset;
            curinter.prevnode=splinter.offset;
            curinter.current_length=max_number_of_children-(max_number_of_children>>1);
            for(size_t i=0;i<curinter.current_length;++i)  curinter.divider[i]=curinter.divider[i+splinter.current_length];
            add_divider(curparent, splinter.divider[0].key, curinter.divider[0].key, splinter.offset, curinter.offset);
            if(key<curinter.divider[0].key)  {internal_node tmp=curinter; curinter=splinter, splinter=tmp;}
            write_data(&curparent, sizeof(internal_node), curparent.offset);
            write_data(&splinter, sizeof(internal_node), splinter.offset);
        }

        //the curleaf change to what the key should be in
        void split_leaf_node(leaf_node &curleaf, const Key &key){
            leaf_node splleaf;
            internal_node curparent;
            if(curleaf.parenode){
                read_data(&curparent, sizeof(internal_node), curleaf.parenode);
                if(curparent.current_length==max_number_of_children)
                    split_internal_node(curparent, curleaf.data[0].key);
            }
            else  {create_parent(curparent, curleaf.data[0].key, curleaf.offset);  curparent.childisleaf=true;  basic_data.rootisleaf=false;}

            splleaf.offset=basic_data.file_length, basic_data.file_length+=sizeof(leaf_node);
            splleaf.prevnode=curleaf.prevnode;
            splleaf.nextnode=curleaf.offset;
            splleaf.parenode=curparent.offset;
            splleaf.current_length=max_number_of_data>>1;
            for(size_t i=0;i<splleaf.current_length;++i)  splleaf.data[i]=curleaf.data[i];
            if(splleaf.prevnode){
                leaf_node tmp;
                read_data(&tmp, sizeof(internal_node), splleaf.prevnode);
                tmp.nextnode=splleaf.offset;
                write_data(&tmp, sizeof(internal_node), splleaf.prevnode);
            }

            curleaf.prevnode=splleaf.offset;
            curleaf.parenode=curparent.offset;
            curleaf.current_length=max_number_of_data-(max_number_of_data>>1);
            for(size_t i=0;i<curleaf.current_length;++i)  curleaf.data[i]=curleaf.data[i+splleaf.current_length];
            add_divider(curparent, splleaf.data[0].key, curleaf.data[0].key, splleaf.offset, curleaf.offset);
            if(basic_data.leaf_offset==curleaf.offset)  basic_data.leaf_offset=splleaf.offset;
            if(key<curleaf.data[0].key)  {leaf_node tmp=curleaf; curleaf=splleaf, splleaf=tmp;}
            write_data(&curparent, sizeof(internal_node), curparent.offset);
            write_data(&splleaf, sizeof(leaf_node), splleaf.offset);
        }

        /***    the following functions are related to deletion    ***/

        template <class T>
        inline void delete_parent(internal_node &curinter){
            T tmp;
            read_data(&tmp, sizeof(T), curinter.divider[0].childpointer);
            tmp.parenode=0, basic_data.root_offset=curinter.divider[0].childpointer;
            write_data(&tmp, sizeof(T), curinter.divider[0].childpointer);
        }

        inline void delete_divider(internal_node &curinter, Key &key){
            size_t mid=find_divider(curinter, key);
            --curinter.current_length;
            if(mid==0)  update_divider_wrapper<internal_node>(curinter, curinter.divider[0].key, curinter.divider[1].key);
            for(size_t i=mid;i<curinter.current_length;++i)  curinter.divider[i]=curinter.divider[i+1];
        }

        inline void judge_internal(internal_node &curinter){
            if(curinter.offset==basic_data.root_offset){
                if(curinter.current_length==1){
                    if(curinter.childisleaf)  {basic_data.rootisleaf=true;  delete_parent<leaf_node>(curinter);}
                    else  delete_parent<internal_node>(curinter);
                }
                else  write_data(&curinter, sizeof(internal_node), curinter.offset);
            }
            else if(curinter.current_length<max_number_of_children>>1){
                internal_node brointer;
                if(curinter.nextnode){
                    read_data(&brointer, sizeof(internal_node), curinter.nextnode);
                    if(brointer.current_length+curinter.current_length<=max_number_of_children)  merge_internal_node(curinter, brointer);
                    else  borrow_right_inter(curinter, brointer);
                }
                else if(curinter.prevnode){
                    read_data(&brointer, sizeof(internal_node), curinter.prevnode);
                    if(brointer.current_length+curinter.current_length<=max_number_of_children)  merge_internal_node(brointer, curinter);
                    else  borrow_left_inter(curinter, brointer);
                }
                else  write_data(&curinter, sizeof(internal_node), curinter.offset);
            }
            else  write_data(&curinter, sizeof(internal_node), curinter.offset);
        }

        inline void borrow_left_inter(internal_node &curinter, internal_node &brointer){
            long borrownum=long((brointer.current_length-(max_number_of_children>>1))>>1);
            update_divider_wrapper<internal_node>(curinter, curinter.divider[0].key, brointer.divider[brointer.current_length-borrownum].key);
            for(size_t i=curinter.current_length-1;i<curinter.current_length;--i)
                curinter.divider[i+borrownum]=curinter.divider[i];
            curinter.current_length+=borrownum, brointer.current_length-=borrownum;
            for(size_t i=0;i<borrownum;++i)
                curinter.divider[i]=brointer.divider[i+brointer.current_length];
            update_parent_wrapper(curinter, 0, borrownum-1);
            write_data(&curinter, sizeof(internal_node), curinter.offset);
            write_data(&brointer, sizeof(internal_node), brointer.offset);
        }

        inline void borrow_right_inter(internal_node &curinter, internal_node &brointer){
            long borrownum=long((brointer.current_length-(max_number_of_children>>1))>>1);
            update_divider_wrapper<internal_node>(brointer, brointer.divider[0].key, brointer.divider[borrownum].key);
            for(size_t i=0;i<borrownum;++i)
                curinter.divider[i+curinter.current_length]=brointer.divider[i];
            update_parent_wrapper(curinter, curinter.current_length, curinter.current_length+borrownum-1);
            curinter.current_length+=borrownum, brointer.current_length-=borrownum;
            for(size_t i=0;i<brointer.current_length;++i)
                brointer.divider[i]=brointer.divider[i+borrownum];
            write_data(&curinter, sizeof(internal_node), curinter.offset);
            write_data(&brointer, sizeof(internal_node), brointer.offset);
        }

        inline void borrow_left_leaf(leaf_node &curleaf, leaf_node &broleaf){
            long borrownum=long((broleaf.current_length-(max_number_of_data>>1))>>1);
            update_divider_wrapper<leaf_node>(curleaf, curleaf.data[0].key, broleaf.data[broleaf.current_length-borrownum].key);
            for(size_t i=curleaf.current_length-1;i<curleaf.current_length;--i)
                curleaf.data[i+borrownum]=curleaf.data[i];
            curleaf.current_length+=borrownum, broleaf.current_length-=borrownum;
            for(size_t i=0;i<borrownum;++i)
                curleaf.data[i]=broleaf.data[i+broleaf.current_length];
            write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            write_data(&broleaf, sizeof(leaf_node), broleaf.offset);
        }

        inline void borrow_right_leaf(leaf_node &curleaf, leaf_node &broleaf){
            long borrownum=long((broleaf.current_length-(max_number_of_data>>1))>>1);
            update_divider_wrapper<leaf_node>(broleaf, broleaf.data[0].key, broleaf.data[borrownum].key);
            for(size_t i=0;i<borrownum;++i)
                curleaf.data[i+curleaf.current_length]=broleaf.data[i];
            curleaf.current_length+=borrownum, broleaf.current_length-=borrownum;
            for(size_t i=0;i<broleaf.current_length;++i)
                broleaf.data[i]=broleaf.data[i+borrownum];
            write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            write_data(&broleaf, sizeof(leaf_node), broleaf.offset);
        }

        //merge the current internal_node and the **next** one
        void merge_internal_node(internal_node &curinter, internal_node &brointer){
            internal_node curparent;
            read_data(&curparent, sizeof(internal_node), brointer.parenode);

            delete_divider(curparent, brointer.divider[0].key);
            for(size_t i=0;i<brointer.current_length;++i)
                curinter.divider[i+curinter.current_length]=brointer.divider[i];
            curinter.current_length+=brointer.current_length;
            update_parent_wrapper(curinter, curinter.current_length-brointer.current_length, curinter.current_length-1);
            curinter.nextnode=brointer.nextnode;
            if(curinter.nextnode){
                internal_node tmp;
                read_data(&tmp, sizeof(internal_node), curinter.nextnode);
                tmp.prevnode=curinter.offset;
                write_data(&tmp, sizeof(internal_node), curinter.nextnode);
            }
            write_data(&curinter, sizeof(internal_node), curinter.offset);
            judge_internal(curparent);
        }

        //merge the current leaf_node and the **next** one
        void merge_leaf_node(leaf_node &curleaf, leaf_node &broleaf){
            internal_node curparent;
            read_data(&curparent, sizeof(internal_node), broleaf.parenode);
            delete_divider(curparent, broleaf.data[0].key);

            for(size_t i=0;i<broleaf.current_length;++i)
                curleaf.data[i+curleaf.current_length]=broleaf.data[i];
            curleaf.current_length+=broleaf.current_length;
            curleaf.nextnode=broleaf.nextnode;
            if(curleaf.nextnode){
                leaf_node tmp;
                read_data(&tmp, sizeof(leaf_node), curleaf.nextnode);
                tmp.prevnode=curleaf.offset;
                write_data(&tmp, sizeof(leaf_node), curleaf.nextnode);
            }
            write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            judge_internal(curparent);
        }

    public:
        BTree() {
            strcpy(file_name, "BPTree_Data");
            if((current_file=fopen(file_name, "rb+"))!=nullptr)  read_data(&basic_data, sizeof(infor), 0);
            else  initialize();
        }

        BTree(const char *fname) {
            strcpy(file_name, fname);
            if((current_file=fopen(file_name, "rb+"))!=nullptr)  read_data(&basic_data, sizeof(infor), 0);
            else  initialize();
        }

        ~BTree() {
            write_data(&basic_data, sizeof(infor), 0);
            fclose(current_file);
        }

        // Clear the BTree  (from delete to run away)
        void clear() {
            initialize();
        }

        bool insert(const Key &key, const Value &value) {
            size_t lowbo;
            leaf_node curleaf;
            read_data(&curleaf, sizeof(leaf_node), findleaf(key));
            if(curleaf.current_length==max_number_of_data)  split_leaf_node(curleaf, key);

            lowbo=find_lowbound(curleaf, key);
            if(lowbo==0)  update_divider_wrapper<leaf_node>(curleaf, curleaf.data[0].key, key);
            if(lowbo!=curleaf.current_length && curleaf.data[lowbo].key==key)  return false;
            for(size_t i=curleaf.current_length++;i>lowbo;--i)  curleaf.data[i]=curleaf.data[i-1];
            curleaf.data[lowbo].key=key;
            curleaf.data[lowbo].value=value;
            write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            return true;
        }

        bool modify(const Key &key, const Value &value) {
            size_t lowbo;
            leaf_node curleaf;
            read_data(&curleaf, sizeof(leaf_node), findleaf(key));
            lowbo=find_lowbound(curleaf, key);
            if(lowbo==curleaf.current_length || curleaf.data[lowbo].key!=key)  return false;
            curleaf.data[lowbo].value=value;
            write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            return true;
        }

        Value at(const Key &key) {
            leaf_node curleaf;
            size_t lowbo;
            read_data(&curleaf, sizeof(leaf_node), findleaf(key));
            lowbo=find_lowbound(curleaf, key);
            if(lowbo==curleaf.current_length || curleaf.data[lowbo].key!=key)  return Value();
            return curleaf.data[lowbo].value;
        }

        bool erase(const Key &key) {
            leaf_node curleaf;
            leaf_node broleaf;
            size_t lowbo;
            read_data(&curleaf, sizeof(leaf_node), findleaf(key));
            lowbo=find_lowbound(curleaf, key);
            if(lowbo==curleaf.current_length || curleaf.data[lowbo].key!=key)  return false;
            if(lowbo==0)  update_divider_wrapper<leaf_node>(curleaf, curleaf.data[0].key, curleaf.data[1].key);

            --curleaf.current_length;
            for(size_t i=lowbo;i<curleaf.current_length;++i)  curleaf.data[i]=curleaf.data[i+1];
            if(curleaf.current_length<max_number_of_data>>1){
                if(curleaf.nextnode){
                    read_data(&broleaf, sizeof(leaf_node), curleaf.nextnode);
                    if(broleaf.current_length+curleaf.current_length<=max_number_of_data)  merge_leaf_node(curleaf, broleaf);
                    else  borrow_right_leaf(curleaf, broleaf);
                }
                else if(curleaf.prevnode){
                    read_data(&broleaf, sizeof(leaf_node), curleaf.prevnode);
                    if(broleaf.current_length+curleaf.current_length<=max_number_of_data)  merge_leaf_node(broleaf, curleaf);
                    else  borrow_left_leaf(curleaf, broleaf);
                }
                else  write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            }
            else  write_data(&curleaf, sizeof(leaf_node), curleaf.offset);
            return true;
        }

        //***ATTENTION**  unpredictable error would occur when using an iterator and insert()/modify()/erase()/another iterator at the same time
        class iterator {
        friend class BTree;
        private:
            // Your private members go here
            leaf_node curleaf;
            FILE *curfile;
            size_t position;

            inline void get_leaf(long off){
                fseek(curfile, off, SEEK_SET);
                if(fread(&curleaf, sizeof(leaf_node), 1, curfile)==0)  throw fail_in_file_IO();
            }

        public:
            // modify by iterator
            bool modify(const Value& value){
                if(curleaf.offset==0)  throw invalid_iterator();
                if(position==curleaf.current_length)  throw index_out_of_bound();
                curleaf.data[position].value=value;
                fseek(curfile, curleaf.offset, SEEK_SET);
                if(fwrite(&curleaf, sizeof(leaf_node), 1, curfile)==0)  throw fail_in_file_IO();
                return true;
            }

            Key getKey() const {
                return curleaf.data[position].key;
            }
            Value getValue() const {
                return curleaf.data[position].value;
            }

            iterator():position(0), curfile(nullptr){}

            iterator(const iterator& other) {
                curleaf=other.curleaf;
                curfile=other.curfile;
                position=other.position;
            }

            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                iterator tmp=*this;
                ++(*this);
                return tmp;
            }
            iterator& operator++() {
                if(curleaf.offset==0)  throw invalid_iterator();
                if(position<curleaf.current_length-1)  ++position;
                else if(curleaf.nextnode==0){
                    if(position==curleaf.current_length-1)  ++position;
                    else  throw index_out_of_bound();
                }
                else{
                    get_leaf(curleaf.nextnode);
                    position=0;
                }
                return *this;
            }

            iterator operator--(int) {
                iterator tmp=*this;
                --(*this);
                return tmp;
            }
            iterator& operator--() {
                if(curleaf.offset==0)  throw invalid_iterator();
                if(position>0)  --position;
                else if(curleaf.prevnode==0)  throw index_out_of_bound();
                else{
                    get_leaf(curleaf.prevnode);
                    position=curleaf.current_length-1;
                }
                return *this;
            }

            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            bool operator==(const iterator& rhs) const {
                return curfile==rhs.curfile && curleaf.offset==rhs.curleaf.offset && position==rhs.position;
            }
            bool operator!=(const iterator& rhs) const {
                return curfile!=rhs.curfile || curleaf.offset!=rhs.curleaf.offset || position!=rhs.position;
            }
        };

        iterator begin() {
            iterator tmp;
            read_data(&tmp.curleaf, sizeof(leaf_node), basic_data.leaf_offset);
            tmp.curfile=current_file;
            tmp.position=0;
            return tmp;
        }

        // return an iterator to the end(the next element after the last)
        iterator end() {
            long leafoff=basic_data.root_offset;
            iterator tmp;
            internal_node curinter;
            if(!basic_data.rootisleaf){
                do{
                    read_data(&curinter, sizeof(internal_node), leafoff);
                    leafoff=curinter.divider[curinter.current_length-1].childpointer;
                }while(!curinter.childisleaf);
            }
            read_data(&tmp.curleaf, sizeof(leaf_node), leafoff);
            tmp.curfile=current_file;
            tmp.position=tmp.curleaf.current_length;
            return tmp;
        }

        iterator find(const Key &key) {
            iterator tmp;
            read_data(&tmp.curleaf, sizeof(leaf_node), findleaf(key));
            tmp.curfile=current_file;
            tmp.position=find_lowbound(tmp.curleaf, key);
            if(tmp.position==tmp.curleaf.current_length || tmp.curleaf.data[tmp.position].key!=key)  return end();  //refer to find() in map
            return tmp;
        }

        // return an iterator whose key is the smallest key greater or equal than 'key'
        iterator lower_bound(const Key &key) {
            iterator tmp;
            read_data(&tmp.curleaf, sizeof(leaf_node), findleaf(key));
            tmp.curfile=current_file;
            tmp.position=find_lowbound(tmp.curleaf, key);
            if(tmp.position==tmp.curleaf.current_length){
                if(tmp.curleaf.nextnode)  ++tmp;  //it works right
                else  return end();  //refer to lower_bound() in map
            }
            return tmp;
        }

    friend class iterator;
    };
}  // namespace sjtu
