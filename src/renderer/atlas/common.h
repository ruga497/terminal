// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <dwrite_3.h>
#include <dxgi1_2.h>

#include <til/generational.h>

#include "../../renderer/inc/IRenderEngine.hpp"

namespace Microsoft::Console::Render::Atlas
{
#define ATLAS_FLAG_OPS(type, underlying)                                                       \
    constexpr type operator~(type v) noexcept                                                  \
    {                                                                                          \
        return static_cast<type>(~static_cast<underlying>(v));                                 \
    }                                                                                          \
    constexpr type operator|(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator&(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr type operator^(type lhs, type rhs) noexcept                                      \
    {                                                                                          \
        return static_cast<type>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs)); \
    }                                                                                          \
    constexpr void operator|=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs | rhs;                                                                       \
    }                                                                                          \
    constexpr void operator&=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs & rhs;                                                                       \
    }                                                                                          \
    constexpr void operator^=(type& lhs, type rhs) noexcept                                    \
    {                                                                                          \
        lhs = lhs ^ rhs;                                                                       \
    }

#define ATLAS_POD_OPS(type)                                    \
    constexpr bool operator==(const type& rhs) const noexcept  \
    {                                                          \
        return __builtin_memcmp(this, &rhs, sizeof(rhs)) == 0; \
    }                                                          \
                                                               \
    constexpr bool operator!=(const type& rhs) const noexcept  \
    {                                                          \
        return !(*this == rhs);                                \
    }

    template<typename T>
    struct vec2
    {
        T x{};
        T y{};

        ATLAS_POD_OPS(vec2)
    };

    template<typename T>
    struct vec4
    {
        T x{};
        T y{};
        T z{};
        T w{};

        ATLAS_POD_OPS(vec4)
    };

    template<typename T>
    struct rect
    {
        T left{};
        T top{};
        T right{};
        T bottom{};

        ATLAS_POD_OPS(rect)

        constexpr bool empty() const noexcept
        {
            return (left >= right) || (top >= bottom);
        }

        constexpr bool non_empty() const noexcept
        {
            return (left < right) && (top < bottom);
        }
    };

    template<typename T>
    struct range
    {
        T start{};
        T end{};

        ATLAS_POD_OPS(range)
    };

    using u8 = uint8_t;

    using u16 = uint16_t;
    using u16x2 = vec2<u16>;
    using u16x4 = vec4<u16>;
    using u16r = rect<u16>;

    using i16 = int16_t;
    using i16x2 = vec2<i16>;
    using i16x4 = vec4<i16>;
    using i16r = rect<i16>;

    using u32 = uint32_t;
    using u32x2 = vec2<u32>;
    using u32x4 = vec4<u32>;
    using u32r = rect<u32>;

    using i32 = int32_t;
    using i32x2 = vec2<i32>;
    using i32x4 = vec4<i32>;
    using i32r = rect<i32>;

    using f32 = float;
    using f32x2 = vec2<f32>;
    using f32x4 = vec4<f32>;
    using f32r = rect<f32>;

    // I wrote `Buffer` instead of using `std::vector`, because I want to convey that these things
    // explicitly _don't_ hold resizeable contents, but rather plain content of a fixed size.
    // For instance I didn't want a resizeable vector with a `push_back` method for my fixed-size
    // viewport arrays - that doesn't make sense after all. `Buffer` also doesn't initialize
    // contents to zero, allowing rapid creation/destruction and you can easily specify a custom
    // (over-)alignment which can improve rendering perf by up to ~20% over `std::vector`.
    template<typename T, size_t Alignment = alignof(T)>
    struct Buffer
    {
        constexpr Buffer() noexcept = default;

        explicit Buffer(size_t size) :
            _data{ allocate(size) },
            _size{ size }
        {
            std::uninitialized_default_construct_n(_data, size);
        }

        Buffer(const T* data, size_t size) :
            _data{ allocate(size) },
            _size{ size }
        {
            // Changing the constructor arguments to accept std::span might
            // be a good future extension, but not to improve security here.
            // You can trivially construct std::span's from invalid ranges.
            // Until then the raw-pointer style is more practical.
#pragma warning(suppress : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
            std::uninitialized_copy_n(data, size, _data);
        }

        ~Buffer()
        {
            destroy();
        }

        Buffer(const Buffer& other) = delete;
        Buffer& operator=(const Buffer& other) = delete;

        Buffer(Buffer&& other) noexcept :
            _data{ std::exchange(other._data, nullptr) },
            _size{ std::exchange(other._size, 0) }
        {
        }

        Buffer& operator=(Buffer&& other) noexcept
        {
            destroy();
            _data = std::exchange(other._data, nullptr);
            _size = std::exchange(other._size, 0);
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return _data != nullptr;
        }

        T& operator[](size_t index) noexcept
        {
            assert(index < _size);
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data[index];
        }

        const T& operator[](size_t index) const noexcept
        {
            assert(index < _size);
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data[index];
        }

        T* data() noexcept
        {
            return _data;
        }

        const T* data() const noexcept
        {
            return _data;
        }

        size_t size() const noexcept
        {
            return _size;
        }

        T* begin() noexcept
        {
            return _data;
        }

        const T* begin() const noexcept
        {
            return _data;
        }

        T* end() noexcept
        {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data + _size;
        }

        const T* end() const noexcept
        {
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
            return _data + _size;
        }

    private:
        // These two functions don't need to use scoped objects or standard allocators,
        // since this class is in fact an scoped allocator object itself.
#pragma warning(push)
#pragma warning(disable : 26402) // Return a scoped object instead of a heap-allocated if it has a move constructor (r.3).
#pragma warning(disable : 26409) // Avoid calling new and delete explicitly, use std::make_unique<T> instead (r.11).
        static T* allocate(size_t size)
        {
            if (!size)
            {
                return nullptr;
            }
            if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                return static_cast<T*>(::operator new(size * sizeof(T)));
            }
            else
            {
                return static_cast<T*>(::operator new(size * sizeof(T), static_cast<std::align_val_t>(Alignment)));
            }
        }

        static void deallocate(T* data) noexcept
        {
            if constexpr (Alignment <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
            {
                ::operator delete(data);
            }
            else
            {
                ::operator delete(data, static_cast<std::align_val_t>(Alignment));
            }
        }
#pragma warning(pop)

        void destroy() noexcept
        {
            std::destroy_n(_data, _size);
            deallocate(_data);
        }

        T* _data = nullptr;
        size_t _size = 0;
    };

    struct TargetSettings
    {
        HWND hwnd = nullptr;
        bool enableTransparentBackground = false;
        bool useSoftwareRendering = false;
    };

    inline constexpr auto DefaultAntialiasingMode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;

    struct FontSettings
    {
        wil::com_ptr<IDWriteFontCollection> fontCollection;
        wil::com_ptr<IDWriteFontFamily> fontFamily;
        std::wstring fontName;
        std::vector<DWRITE_FONT_FEATURE> fontFeatures;
        std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
        f32 fontSize = 0;
        f32 advanceScale = 0;
        u16x2 cellSize;
        u16 fontWeight = 0;
        u16 baseline = 0;
        u16 descender = 0;
        u16 underlinePos = 0;
        u16 underlineWidth = 0;
        u16 strikethroughPos = 0;
        u16 strikethroughWidth = 0;
        u16x2 doubleUnderlinePos;
        u16 thinLineWidth = 0;
        u16 dpi = 96;
        u8 antialiasingMode = DefaultAntialiasingMode;

        std::vector<uint16_t> softFontPattern;
        til::size softFontCellSize;
        size_t softFontCenteringHint = 0;
    };

    struct CursorSettings
    {
        ATLAS_POD_OPS(CursorSettings)

        u32 cursorColor = 0xffffffff;
        u16 cursorType = 0;
        u16 heightPercentage = 20;
    };

    struct MiscellaneousSettings
    {
        u32 backgroundColor = 0;
        u32 selectionColor = 0x7fffffff;
        std::wstring customPixelShaderPath;
        bool useRetroTerminalEffect = false;
    };

    struct Settings
    {
        til::generational<TargetSettings> target;
        til::generational<FontSettings> font;
        til::generational<CursorSettings> cursor;
        til::generational<MiscellaneousSettings> misc;
        u16x2 targetSize;
        u16x2 cellCount;
    };

    using GenerationalSettings = til::generational<Settings>;

    inline GenerationalSettings DirtyGenerationalSettings() noexcept
    {
        return GenerationalSettings{
            til::generation_t{ 1 },
            til::generational<TargetSettings>{ til::generation_t{ 1 } },
            til::generational<FontSettings>{ til::generation_t{ 1 } },
            til::generational<CursorSettings>{ til::generation_t{ 1 } },
            til::generational<MiscellaneousSettings>{ til::generation_t{ 1 } },
        };
    }

    enum class FontRelevantAttributes : u8
    {
        None = 0,
        Bold = 0b01,
        Italic = 0b10,
    };
    ATLAS_FLAG_OPS(FontRelevantAttributes, u8)

    // This fake IDWriteFontFace* is a place holder that is used when we draw DECDLD/DRCS soft fonts. It's wildly
    // invalid C++, but I wrote the alternative, proper code with bitfields/flags and such and it turned into a
    // bigger mess than this violation against the C++ consortium's conscience. It also didn't help BackendD3D,
    // which hashes FontFace and an additional flag field would double the hashmap key size due to padding.
    // It's a macro, because constexpr doesn't work here in C++20 and regular "const" doesn't inline.
#define IDWriteFontFace_SoftFont (static_cast<IDWriteFontFace*>(nullptr) + 1)

    // The existence of IDWriteFontFace_SoftFont unfortunately requires us to reimplement wil::com_ptr<IDWriteFontFace>.
    //
    // Unfortunately this code seems to confuse MSVC's linter? The 3 smart pointer warnings are somewhat funny.
    // It doesn't understand that this class is a smart pointer itself. The other 2 are valid, but don't apply here.
#pragma warning(push)
#pragma warning(disable : 26415) // Smart pointer parameter 'other' is used only to access contained pointer. Use T* or T& instead (r.30).
#pragma warning(disable : 26416) // Shared pointer parameter 'other' is passed by rvalue reference. Pass by value instead (r.34).
#pragma warning(disable : 26418) // Shared pointer parameter 'other' is not copied or moved. Use T* or T& instead (r.36).
#pragma warning(disable : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).)
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    struct FontFace
    {
        FontFace() = default;

        ~FontFace() noexcept
        {
            _release();
        }

        FontFace(const FontFace& other) noexcept :
            FontFace{ other.get() }
        {
        }

        FontFace(FontFace&& other) noexcept :
            _ptr{ other.detach() }
        {
        }

        FontFace& operator=(const FontFace& other) noexcept
        {
            _release();
            _ptr = other.get();
            _addRef();
            return *this;
        }

        FontFace& operator=(FontFace&& other) noexcept
        {
            _release();
            _ptr = other.detach();
            return *this;
        }

        FontFace(IDWriteFontFace* ptr) noexcept :
            _ptr{ ptr }
        {
            _addRef();
        }

        FontFace(const wil::com_ptr<IDWriteFontFace>& other) noexcept :
            FontFace{ other.get() }
        {
        }

        FontFace(wil::com_ptr<IDWriteFontFace>&& other) noexcept :
            _ptr{ other.detach() }
        {
        }

        void attach(IDWriteFontFace* other) noexcept
        {
            _release();
            _ptr = other;
        }

        [[nodiscard]] IDWriteFontFace* detach() noexcept
        {
            const auto tmp = _ptr;
            _ptr = nullptr;
            return tmp;
        }

        IDWriteFontFace* get() const noexcept
        {
            return _ptr;
        }

        bool is_proper_font() const noexcept
        {
            return _ptr > IDWriteFontFace_SoftFont;
        }

    private:
        void _addRef() const noexcept
        {
            if (is_proper_font())
            {
                _ptr->AddRef();
            }
        }

        void _release() const noexcept
        {
            if (is_proper_font())
            {
                _ptr->Release();
            }
        }

        IDWriteFontFace* _ptr = nullptr;
    };
#pragma warning(pop)

    struct FontMapping
    {
        FontFace fontFace;
        f32 fontEmSize = 0;
        u32 glyphsFrom = 0;
        u32 glyphsTo = 0;
    };

    struct GridLineRange
    {
        GridLineSet lines;
        u32 color = 0;
        u16 from = 0;
        u16 to = 0;
    };

    struct ShapedRow
    {
        void clear(u16 y, u16 cellHeight) noexcept
        {
            mappings.clear();
            glyphIndices.clear();
            glyphAdvances.clear();
            glyphOffsets.clear();
            colors.clear();
            gridLineRanges.clear();
            lineRendition = LineRendition::SingleWidth;
            selectionFrom = 0;
            selectionTo = 0;
            dirtyTop = y * cellHeight;
            dirtyBottom = dirtyTop + cellHeight;
        }

        std::vector<FontMapping> mappings;
        std::vector<u16> glyphIndices;
        std::vector<f32> glyphAdvances; // same size as glyphIndices
        std::vector<DWRITE_GLYPH_OFFSET> glyphOffsets; // same size as glyphIndices
        std::vector<u32> colors; // same size as glyphIndices
        std::vector<GridLineRange> gridLineRanges;
        LineRendition lineRendition = LineRendition::SingleWidth;
        u16 selectionFrom = 0;
        u16 selectionTo = 0;
        til::CoordType dirtyTop = 0;
        til::CoordType dirtyBottom = 0;
    };

    struct RenderingPayload
    {
        //// Parameters which are constant across backends.
        wil::com_ptr<ID2D1Factory> d2dFactory;
        wil::com_ptr<IDWriteFactory2> dwriteFactory;
        wil::com_ptr<IDWriteFactory4> dwriteFactory4; // optional, might be nullptr
        wil::com_ptr<IDWriteFontFallback> systemFontFallback;
        wil::com_ptr<IDWriteFontFallback1> systemFontFallback1; // optional, might be nullptr
        wil::com_ptr<IDWriteTextAnalyzer1> textAnalyzer;
        wil::com_ptr<IDWriteRenderingParams1> renderingParams;
        std::function<void(HRESULT)> warningCallback;
        std::function<void(HANDLE)> swapChainChangedCallback;

        //// Parameters which are constant for the existence of the backend.
        struct
        {
            wil::com_ptr<IDXGIFactory2> factory;
            wil::com_ptr<IDXGIAdapter1> adapter;
            LUID adapterLuid{};
            UINT adapterFlags = 0;
        } dxgi;

        //// Parameters which change seldom.
        GenerationalSettings s;

        //// Parameters which change every frame.
        // This is the backing buffer for `rows`.
        Buffer<ShapedRow> unorderedRows;
        // This is used as a scratch buffer during scrolling.
        Buffer<ShapedRow*> rowsScratch;
        Buffer<ShapedRow*> rows;
        // This stride (width) of the backgroundBitmap is a "count" of u32 and not in bytes.
        size_t backgroundBitmapStride = 0;
        Buffer<u32, 32> backgroundBitmap;
        // 1 ensures that the backends redraw the background, even if the background is
        // entirely black, just like `backgroundBitmap` is all back after it gets created.
        til::generation_t backgroundBitmapGeneration{ 1 };

        u16r cursorRect;

        til::rect dirtyRectInPx;
        u16x2 invalidatedRows;
        i16 scrollOffset = 0;

        void MarkAllAsDirty() noexcept
        {
            dirtyRectInPx = { 0, 0, s->targetSize.x, s->targetSize.y };
            invalidatedRows = { 0, s->cellCount.y };
            scrollOffset = 0;
        }
    };

    struct IBackend
    {
        virtual ~IBackend() = default;
        virtual void Render(RenderingPayload& payload) = 0;
        virtual bool RequiresContinuousRedraw() noexcept = 0;
        virtual void WaitUntilCanRender() noexcept = 0;
    };

}