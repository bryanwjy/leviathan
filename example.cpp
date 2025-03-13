

class MyClass {

public:
    MyClass(std::string_view, int);
    int get() const noexcept;
    void set(int);
    std::string_view name() const noexcept;

    int add_person(struct Person);

    static constexpr int Constant = 0;

private:
};

DEFINE_PYTHON_MODULE(my_module);

template <auto>
struct uses {};

struct foo : basic_object<MyClass> {
    // MACRO BEGIN
    template <argument::declaration... Ts>
    using arguments = lev::argument::tuple<Ts...>;

    template <std::same_as<void> R, argument::declaration T,
        argument::declaration... Ts>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<
            adapted_constructor<foo, default_throw, R, T, Ts...>>)
    R init(arguments<T, Ts...>);

    template <method::decorator auto D, std::same_as<void> R,
        argument::declaration T, argument::declaration... Ts>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, D, R, T, Ts...>>)
    R init(arguments<T, Ts...>) noexcept(method::decorated_with<nothrow, D>);

    template <std::same_as<result_code> R, argument::declaration T,
        argument::declaration... Ts>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, nothrow, R, T, Ts...>>)
    R init(arguments<T, Ts...>) noexcept;

    template <method::decorator auto D, std::same_as<result_code> R,
        argument::declaration T, argument::declaration... Ts>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, D | nothrow, R, T, Ts...>>)
    R init(arguments<T, Ts...>) noexcept;

    template <std::same_as<void> R>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, default_throw, R>>)
    R init();

    template <method::decorator auto D, std::same_as<void> R>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, D, R>>)
    R init() noexcept(method::decorated_with<nothrow, D>);

    template <std::same_as<result_code> R>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, nothrow, R>>)
    R init() noexcept;

    template <method::decorator auto D, std::same_as<result_code> R>
    requires (!method::decorated_with<static_method, D> &&
        define_initializer<adapted_constructor<foo, D | nothrow, R>>)
    R init(arguments<T, Ts...>) noexcept;

    template <std::same_as<python_ptr<foo>> R, argument::declaration T,
        argument::declaration... Ts>
    requires define_initializer<
        adapted_constructor<foo, static_method, R, T, Ts...>>
    static R init(arguments<T, Ts...>);

    template <method::decorator auto D, std::same_as<python_ptr<foo>> R,
        argument::declaration T, argument::declaration... Ts>
    requires define_initializer<
        adapted_constructor<foo, D | static_method, R, T, Ts...>>
    static R init(arguments<T, Ts...>) noexcept(
        method::decorated_with<nothrow, D>);

    template <std::same_as<python_ptr<foo>> R, argument::declaration T>
    requires define_initializer<adapted_constructor<foo, static_method, R>>
    static R init();

    template <method::decorator auto D, std::same_as<python_ptr<foo>> R>
    requires define_initializer<adapted_constructor<foo, D | static_method, R>>
    static R init() noexcept(method::decorated_with<nothrow, D>);

    // define here
    template <string_literal auto Name, typename R, argument::declaration T,
        argument::declaration... Ts>
    requires add_adapted_method<
        adapted_method<foo, Name, vectorcall, R, T, Ts...>>::value
    R def(arguments<T, Ts...>) const;

    template <string_literal auto Name, typename R, argument::declaration T,
        argument::declaration... Ts>
    requires add_adapted_method<
        adapted_method<foo, Name, vectorcall, R, T, Ts...>>::value
    R def(arguments<T, Ts...>);

    template <string_literal auto Name, method::decorator auto D, typename R,
        argument::declaration T, argument::declaration... Ts>
    requires add_adapted_method<
        adapted_method<foo, Name, D, R, T, Ts...>>::value
    R def(arguments<T, Ts...>) const
        noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name, method::decorator auto D, typename R,
        argument::declaration T, argument::declaration... Ts>
    requires add_adapted_method<
        adapted_method<foo, Name, D, R, T, Ts...>>::value
    R def(arguments<T, Ts...>) noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name,
        method::decorated_with<static_method> auto D, typename R,
        argument::declaration T, argument::declaration... Ts>
    requires add_adapted_method<
        adapted_method<foo, Name, D, R, T, Ts...>>::value
    static R def(arguments<T, Ts...>) noexcept(
        method::decorated_with<nothrow, D>);

    template <string_literal auto Name, typename R>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall, R>>::value
    R def() const;

    template <string_literal auto Name, typename R>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall, R>>::value
    R def();

    template <string_literal auto Name, method::decorator auto D, typename R>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    R def() const noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name, method::decorator auto D, typename R>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    R def() noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name,
        method::decorated_with<static_method> auto D, typename R>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    static R def() noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name, auto F>
    requires add_direct_method<Name, foo, F>
    static auto def(uses<F>); // noexcept implied by F, static-ness implied by
                              // signature of F, no keywords allowed

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        std::span<PyObject* const> kwargs, std::span<PyObject* const> kwnames);

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        std::span<PyObject* const> kwargs,
        std::span<PyObject* const> kwnames) const;

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        std::span<PyObject* const> kwargs,
        std::span<PyObject* const>
            kwnames) noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        std::span<PyObject* const> kwargs,
        std::span<PyObject* const> kwnames) const
        noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name,
        method::decorated_with<static_method> auto D>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    static python_ptr<PyObject> def(std::span<PyObject* const> args,
        std::span<PyObject* const> kwargs,
        std::span<PyObject* const>
            kwnames) noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, basic_call,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args, str_dict kwargs);

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, basic_call,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(
        std::span<PyObject* const> args, str_dict kwargs) const;

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        str_dict kwargs) noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args,
        str_dict kwargs) const noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name,
        method::decorated_with<static_method> auto D>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    static python_ptr<PyObject> def(std::span<PyObject* const> args,
        str_dict kwargs) noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args);

    template <string_literal auto Name>
    requires add_adapted_method<adapted_method<foo, Name, vectorcall,
        python_ptr<PyObject>, noconvert>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args) const;

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args) noexcept(
        method::decorated_with<nothrow, D>);

    template <string_literal auto Name, method::decorator auto D>
    requires add_adapted_method<
        adapted_method<foo, Name, D, python_ptr<PyObject>>>::value
    python_ptr<PyObject> def(std::span<PyObject* const> args) const
        noexcept(method::decorated_with<nothrow, D>);

    template <string_literal auto Name,
        method::decorated_with<static_method> auto D>
    requires add_adapted_method<adapted_method<foo, Name, D, R>>::value
    static python_ptr<PyObject> def(std::span<PyObject* const> args) noexcept(
        method::decorated_with<nothrow, D>);
    // MACRO END

    template <>
    void init(arguments<arg<std::string_view>("name"_a), arg<unsigned>("age"_a)>
            args) noexcept {
        //
    }

    template <>
    python_ptr<foo> init<nothrow>(
        arguments<arg<std::string_view>("name"_a), arg<unsigned>("age"_a)>
            args) noexcept {
        //
    }

    template <>
    auto def<"func"_str>(arguments<arg<unsigned>("key"_a),
        arg<float>("value"_a), arg<float>("value2"_a) = opt>
            args) const -> int {
        {
            auto const& [key, value] = args.mandatory();
            auto const& [value2] = args.optional();
            if (value2) {
                return self().func(key, value, *value2);
            } else {
                return self().func(key, value);
            }
        }

        {
            auto [key, value, value2] = args.all();
            if (value2) {
                return self().func(key, value, *value2);
            } else {
                return self().func(key, value);
            }
        }

        {
            if (args.has_value<2>()) {
                return self().func(get<0>(args), get<1>(args), *get<2>(args));
            } else {
                return self().func(get<0>(args), get<1>(args));
            }
        }

        {
            if (args.has_value<"value2"_str>()) {
                return self().func(get<"key"_str>(args), get<"value"_str>(args),
                    *get<"value2"_str>(args));
            } else {
                return self().func(
                    get<"key"_str>(args), get<"value"_str>(args));
            }
        }

        return 0;
    }
};
