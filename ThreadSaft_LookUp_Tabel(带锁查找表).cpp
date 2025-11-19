template<typename Key, typename Value, typename Hash = std::hash<Key>>
class ThreadSaft_LookUp_Tabel {
private:
    class Bucket {
        friend class ThreadSaft_LookUp_Tabel;
    private:
        typedef std::pair<Key, Value> _bucket_value;
        typedef std::list<_bucket_value> _bucket_data;
        typedef typename _bucket_data::iterator _bucket_iterator;
        typedef typename _bucket_data::const_iterator _bucket_const_iterator;

        _bucket_data _data;
        mutable std::shared_mutex _mutex;

        _bucket_iterator get_iterator(const Key& key){
            return std::find_if(_data.begin(), _data.end(), [&](const _bucket_value& it) {
                return it.first == key;
            });
        }

        _bucket_const_iterator get_iterator(const Key& key) const {
            return std::find_if(_data.cbegin(), _data.cend(),
                [&](const _bucket_value& it) { return it.first == key; });
        }

    public:
        Value find_mapped(const Key& key, const Value &default_value) const {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            auto it = get_iterator(key);
            return it == _data.end() ? default_value : it->second;
        }

        void insert_mapped(const Key& key, const Value& value) {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            auto it = get_iterator(key);
            if (it != _data.end()) {
                it->second = value;
            }else {
                _data.push_back(_bucket_value(key, value));
            }
        }

        bool contains_mapped(const Key& key) const {
            std::shared_lock<std::shared_mutex> lock(_mutex);
            auto  it = get_iterator(key);
            return it != _data.end();
        }

        void remove_mapped(const Key& key) {
            std::unique_lock<std::shared_mutex> lock(_mutex);
            auto it = get_iterator(key);
            if (it != _data.end()) {
                _data.erase(it);
            }
        }
    };

private:
    std::vector<std::unique_ptr<Bucket>> _buckets;
    Hash _hasher;

    [[nodiscard]] Bucket &get_bucket_index(const Key& key) const{
        return *_buckets[_hasher(key) % _buckets.size()];
    }

public:
    explicit ThreadSaft_LookUp_Tabel(const unsigned int &size = 19, const Hash &hasher = Hash()) : _buckets(size), _hasher(hasher) {
        for (auto& bucket : _buckets) {
            bucket = std::make_unique<Bucket>();
        }
    }

    ThreadSaft_LookUp_Tabel(const ThreadSaft_LookUp_Tabel&) = delete;
    ThreadSaft_LookUp_Tabel& operator=(const ThreadSaft_LookUp_Tabel&) = delete;

    Value find_mapped(const Key& key, const Value& default_value) const {
        return get_bucket_index(key).find_mapped(key, default_value);
    }

    void insert(const Key& key, const Value& value) {
        get_bucket_index(key).insert_mapped(key, value);
    }

    void remove_mapped(const Key& key) {
        get_bucket_index(key).remove_mapped(key);
    }

    bool contains_mapped(const Key& key) const {
        return get_bucket_index(key).contains_mapped(key);
    }
};
