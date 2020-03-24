#ifndef DICT_H
#define DICT_H

#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <variant>
#include <vector>

namespace cppdict
{
template<typename T1, typename... T2>
constexpr bool is_in()
{
    return std::disjunction_v<std::is_same<T1, T2>...>;
}

struct NoValue
{
};

namespace // Visitor details
{
    struct values_only_t
    {
    };
    struct all_nodes_t
    {
    };
    template<class _Tp>
    struct is_visit_policy : std::false_type
    {
    };
    template<>
    struct is_visit_policy<all_nodes_t> : std::true_type
    {
    };
    template<>
    struct is_visit_policy<values_only_t> : std::true_type
    {
    };
    template<typename T>
    inline constexpr bool is_values_only_v = std::is_same_v<T, values_only_t>;
    template<typename... Ts>

    struct Visitor : Ts...
    {
        Visitor(const Ts&... args)
            : Ts(args)...
        {
        }

        using Ts::operator()...;
    };

    template<typename... Ts>
    auto make_visitor(Ts... lambdas)
    {
        return Visitor<Ts...>(lambdas...);
    }

    template<typename visit_policy_t, typename NodeT, typename... Ts>
    void visit_impl(NodeT& node, Ts... lambdas)
    {
        if (node.isNode())
        {
            for (const auto& [_key, child_node] : std::get<typename NodeT::map_t>(node.data))
            {
                auto const& key = _key;
                if (!is_values_only_v<visit_policy_t> or child_node->isValue())
                {
                    std::visit(
                        [key, lambdas...](auto&& value) {
                            using T = std::decay_t<decltype(value)>;
                            if constexpr (NodeT::template is_value_v<
                                              T> or !is_values_only_v<visit_policy_t>)
                                make_visitor(lambdas...)(key, value);
                        },
                        child_node->data);
                }
            }
        }
        else
            throw std::runtime_error("cppdict: can only visit node");
    }
} // namespace

constexpr values_only_t visit_values_only;
constexpr all_nodes_t visit_all_nodes;

template<typename... Types>
struct Dict
{
    using node_ptr = std::shared_ptr<Dict>;
    using map_t    = std::map<std::string, node_ptr>;
    using data_t   = std::variant<NoValue, map_t, Types...>;

    template<typename T>
    struct is_value : std::conditional<!std::is_same_v<T, NoValue> and !std::is_same_v<T, map_t>,
                                       std::true_type, std::false_type>::type
    {
    };
    template<typename T>
    static constexpr bool is_value_v = is_value<T>::value;


    data_t data = NoValue{};
#ifndef NDEBUG
    static inline std::string currentKey;
#endif

    Dict()       = default;
    Dict(Dict&&) = default;
    Dict(const Dict& other)
        : data{other.data}
    {
        this->copy_data_();
    }

    Dict& operator=(const Dict& other)
    {
        this->copy_data_();
        return *this;
    }
    Dict& operator=(Dict&& other) = default;
    Dict& operator[](const std::string& key)
    {
#ifndef NDEBUG
        currentKey = key;
#endif
        if (isNode())
        {
            // std::cout << key << "\n";
            auto& map = std::get<map_t>(data);

            if (std::end(map) == map.find(key))
            {
                map[key] = std::make_shared<Dict>();
            }

            return *std::get<map_t>(data)[key];
        }
        else if (isEmpty())
        {
            data      = map_t{};
            auto& map = std::get<map_t>(data);
            map[key]  = std::make_shared<Dict>();

            return *std::get<map_t>(data)[key];
        }

        throw std::runtime_error("cppdict: invalid key: " + key);
    }


    bool isLeaf() const noexcept { return !isNode() && !isEmpty(); }

    bool isNode() const noexcept { return std::holds_alternative<map_t>(data); }

    bool isEmpty() const noexcept { return std::holds_alternative<NoValue>(data); }

    bool isValue() const noexcept { return !isNode() and !isEmpty(); }

    template<typename T, typename U = std::enable_if_t<is_in<T, Types...>()>>
    Dict& operator=(const T& value)
    {
        data = value;
        return *this;
    }

    template<typename T>
    T& to()
    {
        if (std::holds_alternative<T>(data))
            return std::get<T>(data);

#ifndef NDEBUG
        std::cout << __FILE__ << " " << __LINE__ << " " << currentKey << std::endl;
#endif
        throw std::runtime_error("cppdict: to<T> invalid type");
    }

    template<typename T>
    T& to(T&& defaultValue)
    {
        if (std::holds_alternative<T>(data))
        {
            return std::get<T>(data);
        }
        else if (isEmpty())
        {
            return defaultValue;
        }

#ifndef NDEBUG
        std::cout << __FILE__ << " " << __LINE__ << " " << currentKey << std::endl;
#endif
        throw std::runtime_error("cppdict: not a map or not default");
    }

    bool contains(std::string key)
    {
        if (std::holds_alternative<map_t>(data))
            return std::get<map_t>(data).count(key) > 0;

#ifndef NDEBUG
        std::cout << __FILE__ << " " << __LINE__ << " " << currentKey << std::endl;
#endif

        throw std::runtime_error("cppdict: contains() has no a map");
    }

    std::size_t size() const noexcept
    {
        if (isNode())
            return std::size(std::get<map_t>(data));
        if (isEmpty())
            return 0;
        return 1;
    }

    decltype(auto) begin()
    {
        if (isNode())
            return std::begin(std::get<map_t>(data));
        else
            throw std::runtime_error("cppdict: can't iterate this node");
    }

    decltype(auto) begin() const noexcept
    {
        if (isNode())
            return std::begin(std::get<map_t>(data));
        else
            throw std::runtime_error("cppdict: can't iterate this node");
    }

    decltype(auto) end()
    {
        if (isNode())
            return std::end(std::get<map_t>(data));
        else
            throw std::runtime_error("cppdict: can't iterate this node");
    }

    decltype(auto) end() const noexcept
    {
        if (isNode())
            return std::end(std::get<map_t>(data));
        else
            throw std::runtime_error("cppdict: can't iterate this node");
    }


    template<class visit_policy_t, typename... Ts,
             std::enable_if_t<is_visit_policy<visit_policy_t>::value, int> = 0>
    void visit(visit_policy_t visit_policy, Ts... lambdas)
    {
        visit_impl<visit_policy_t>(*this, std::forward<Ts>(lambdas)...);
    }

    template<typename... Ts>
    void visit(Ts... lambdas)
    {
        visit_impl<values_only_t>(*this, std::forward<Ts>(lambdas)...);
    }

    template<typename... Ts>
    void visit_leaves(Ts... lambdas)
    {
        if (isNode())
            for (const auto& [_key, node] : std::get<map_t>(data))
            {
                const auto& key = _key;
                if (node->isNode())
                {
                    node->visit_leaves(std::forward<Ts>(lambdas)...);
                }
                else if (node->isLeaf())
                {
                    std::visit(
                        [key, lambdas...](auto&& value) { make_visitor(lambdas...)(key, value); },
                        node->data);
                }
            }
        else
            throw std::runtime_error("cppdict: can only visit node");
    }

private:
    void copy_data_()
    {
        if (isNode())
        {
            auto& my_data = std::get<map_t>(data);
            for (const auto& [key, value] : my_data)
            {
                my_data[key] = std::make_shared<Dict>(*value.get());
            }
        }
    }
};



template<typename... Types>
auto& get(std::vector<std::string> keys, size_t iKey, Dict<Types...>& currentNode)
{
    if (iKey == keys.size() - 1)
        return currentNode[keys[iKey]];

    return get(keys, iKey + 1, currentNode[keys[iKey]]);
}




template<typename T, template<typename... Types> class Dict, typename... Types,
         typename Check = std::enable_if_t<is_in<T, Types...>()>>
void add(std::string path, T&& value, Dict<Types...>& dict)
{
    std::vector<std::string> keys;
    std::string key;
    std::istringstream tokenStream{path};
    while (std::getline(tokenStream, key, '/'))
    {
        keys.push_back(key);
    }

    auto&& node = get(keys, 0ul, dict);
    node        = std::forward<T>(value);
}



} // namespace cppdict
#endif
