#ifndef __Matr_hpp_
#define __Matr_hpp_

#include "def.hpp"
#include "row.hpp"
#include "units.hpp"

namespace mr
{
  // forward declarations
  template <ArithmeticT T, std::size_t N>
    class Matr;

  // common aliases
  template <ArithmeticT T>
    using Matr4 = Matr<T, 4>;

  using Matr4f = Matr4<float>;
  using Matr4d = Matr4<double>;
  using Matr4i = Matr4<int>;
  using Matr4u = Matr4<uint32_t>;

  template <ArithmeticT T, std::size_t N>
    class [[nodiscard]] Matr
    {
    public:
      using RowT = Row<T, N>;

      constexpr Matr() noexcept = default;

      template <typename... Args>
        requires (std::is_same_v<Args, RowT> && ...) &&
                 (sizeof...(Args) == N)
        constexpr Matr(Args... args) noexcept {
          _data = std::array<RowT, N>({args...});
        }

      constexpr Matr(std::array<RowT, N> rows) : _data(std::move(rows)) {}

      template <typename... Args>
        requires (std::is_convertible_v<Args, T> && ...) &&
                 (sizeof...(Args) == N * N)
        constexpr Matr(Args... args) noexcept {
          std::array<T, N * N> tmp {static_cast<T>(args)...};
          for (size_t i = 0; i < N; i++) {
            _data[i] = RowT {
              tmp[N * i + 0],
              tmp[N * i + 1],
              tmp[N * i + 2],
              tmp[N * i + 3]
            };
          }
        }

      // copy semantics
      constexpr Matr(const Matr &) noexcept = default;
      constexpr Matr & operator=(const Matr &) noexcept = default;

      // move semantics
      constexpr Matr(Matr &&) noexcept = default;
      constexpr Matr & operator=(Matr &&) noexcept = default;

      constexpr ~Matr() = default;

      // basic math operations
      constexpr Matr & operator*=(const Matr &other) noexcept {
        std::array<RowT, N> tmp {};
        for (size_t i = 0; i < N; i++) {
          for (size_t j = 0; j < N; j++) {
            tmp[j] += other._data[i] * _data[j][i];
          }
        }
        *this = tmp;
        return *this;
      }

      constexpr Matr & operator+=(const Matr &other) noexcept {
        for (int i = 0; i < N; i++) {
          _data[i] += other._data[i];
        }
        return *this;
      }

      constexpr Matr & operator-=(const Matr &other) noexcept {
        for (int i = 0; i < N; i++) {
          _data[i] -= other._data[i];
        }
        return *this;
      }

      constexpr Matr operator*(const Matr &other) const noexcept {
        std::array<RowT, N> tmp{};
        for (size_t i = 0; i < N; i++) {
          for (size_t j = 0; j < N; j++) {
            tmp[j] += other._data[i] * _data[j][i];
          }
        }
        return {tmp};
      }

      constexpr Matr operator+(const Matr &other) const noexcept {
        std::array<RowT, N> tmp;
        for (size_t i = 0; i < N; i++) {
          tmp[i] = static_cast<RowT>(_data[i] + other._data[i]);
        }
        return {tmp};
      }

      constexpr Matr operator-(const Matr &other) const noexcept {
        std::array<RowT, N> tmp;
        for (size_t i = 0; i < N; i++) {
          tmp[i] = _data[i] - other._data[i];
        }
        return {tmp};
      }

      // matrix related operations
      [[nodiscard]] constexpr const RowT & operator[](size_t i) const noexcept {
        return _data[i];
      }

      [[nodiscard]] constexpr T determinant() const noexcept {
        std::array<RowT, N> tmp = _data;
        T det = 1;

        tmp[N - 1] /= tmp[N - 1][N - 1];
        for (size_t i = 1; i < N; i++) {
          det *= tmp[i - 1][i - 1];
          tmp[i - 1] /= std::abs(tmp[i - 1][i - 1]) <= _epsilon ? static_cast<T>(1) : tmp[i - 1][i - 1];
          for (size_t j = i; j < N; j++) {
            tmp[i] -= tmp[i - 1] * tmp[j][i - 1];
          }
        }
        return det;
      }

      [[nodiscard]] constexpr T operator!() const noexcept {
        return determinant();
      }

      constexpr Matr transposed() const noexcept {
        std::array<RowT, N> tmp1;
        for (size_t i = 0; i < N; i++) {
          tmp1[i] = SimdImpl<T, N>([this, i](auto j){ return _data[j][i]; });
        }
        return {tmp1};
      }

      constexpr Matr & transpose() noexcept {
        for (size_t i = 0; i < N; i++) {
          _data[i] = SimdImpl<T, N>([this, i](auto j){ return _data[j][i]; });
        }
        return *this;
      }

      constexpr Matr inversed() const noexcept {
        constexpr auto io = std::ranges::iota_view{(size_t)0, N};

        std::array<Row<T, 2 * N>, N> tmp;
        std::for_each(std::execution::par_unseq, io.begin(), io.end(),
                      [&tmp, this](auto i) {
                        // adding temporary variable here brings performance down 2.5 times (reason unknown)
                        tmp[i] += stdx::static_simd_cast<SimdImpl<T, 2 * N>>(stdx::concat(_data[i]._data, identity[i]._data));
                      });

        // null bottom triangle
        for (size_t i = 1; i < N; i++) {
          tmp[i - 1] /= tmp[i - 1][i - 1];
          for (size_t j = i; j < N; j++) {
            tmp[j] -= std::abs(tmp[i - 1][i - 1]) <= _epsilon ? static_cast<T>(0) : tmp[i - 1] * tmp[j][i - 1];
          }
        }
        tmp[N - 1] /= tmp[N - 1][N - 1];

        // null top triangle
        for (int i = N - 2; i >= 0; i--) {
          for (int j = i; j >= 0; j--) {
            tmp[j] -= std::abs(tmp[i + 1][i + 1]) <= _epsilon ? static_cast<T>(0) : tmp[i + 1] * tmp[j][i + 1];
          }
        }

        std::array<RowT, N> res;
        std::for_each(std::execution::par_unseq, io.begin(), io.end(),
          [&tmp, &res](auto i) {
            auto [_, b] = stdx::split<N, N>(tmp[i]._data);
            res[i] = stdx::static_simd_cast<SimdImpl<T, N>>(b);
          });

        return res;
      }

      constexpr Matr & inverse() const noexcept {
        *this = inversed();
        return *this;
      }

      static constexpr Matr4<T> scale(const Vec3<T> &vec) noexcept {
        return Matr4<T> {
          vec.x(),       0,       0, 0,
                0, vec.y(),       0, 0,
                0,       0, vec.z(), 0,
                0,       0,       0, 1
        };
      }

      static constexpr Matr4<T> translate(const Vec3<T> &vec) noexcept {
        return Matr4<T> {
                1,       0,       0, 0,
                0,       1,       0, 0,
                0,       0,       1, 0,
          vec.x(), vec.y(), vec.z(), 1
        };
      }

      static constexpr Matr4<T> rotate_x(const Radians<T> &rad) noexcept {
        T co = std::cos(rad._data);
        T si = std::sin(rad._data);

        return Matr4<T> {
          1,   0,  0, 0,
          0,  co, si, 0,
          0, -si, co, 0,
          0,   0,  0, 1
        };
      }

      static constexpr Matr4<T> rotate_y(const Radians<T> &rad) noexcept {
        T co = std::cos(rad._data);
        T si = std::sin(rad._data);

        return Matr4<T> {
          co, 0, -si, 0,
           0, 1,   0, 0,
          si, 0,  co, 0,
           0, 0,   0, 1
        };
      }

      static constexpr Matr4<T> rotate_z(const Radians<T> &rad) noexcept {
        T co = std::cos(rad._data);
        T si = std::sin(rad._data);

        return Matr4<T> {
           co, si, 0, 0,
          -si, co, 0, 0,
            0,  0, 1, 0,
            0,  0, 0, 1
        };
      }

      static constexpr Matr4<T> rotate(const Radians<T> &rad, const Norm<T, 3> &n) noexcept {
        T co = std::cos(rad._data);
        T si = std::sin(rad._data);
        T nco = 1 - co;

        Matr4<T> tmp1 = scale(n * n * nco + Vec<T, 3>{co});
        Matr4<T> tmp2 = Matr4<T> {
                            0, n.x() * n.y() * nco, n.x() * n.z() * nco, 0,
          n.x() * n.y() * nco,                   0, n.y() * n.z() * nco, 0,
          n.x() * n.z() * nco, n.y() * n.z() * nco,                   0, 0,
                            0,                   0,                   0, 0
        };
        Matr4<T> tmp3 = Matr4<T> {
                    0, -n.z() * si,  n.y() * si, 0,
           n.z() * si,           0, -n.x() * si, 0,
          -n.y() * si,  n.x() * si,           0, 0,
                    0,           0,           0, 0
        };

        return tmp1 + tmp2 + tmp3;
      }

      friend std::ostream & operator<<(std::ostream &os, const Matr &m) noexcept {
        os << "\n(" << m[0] << ",\n";
          for (size_t i = 1; i < N - 1; i++)
            os << ' ' << m[i] << ",\n";
          os << ' ' << m[N - 1] << ')';
        return os;
      }

    private:
      static Matr _identity() {
        std::array<RowT, N> id;
        constexpr auto io = std::ranges::iota_view {(size_t)0, N};

        std::transform(std::execution::par_unseq,
          io.begin(), io.end(), id.begin(),
          [&io](auto i) -> RowT {
            return SimdImpl<T, N>([i](auto i2) { return i2 == i ? 1 : 0; });
          });

        return id;
      }

    public:
      inline static const Matr identity = _identity();
      std::array<RowT, N> _data;

    private:
      static constexpr T _epsilon = std::numeric_limits<T>::epsilon();
    };
} // namespace mr

#ifdef __cpp_lib_format
// std::format support
namespace std {
  template <mr::ArithmeticT T, size_t N>
    struct formatter<mr::Matr<T, N>> {
      template<typename ParseContext>
        constexpr auto parse(ParseContext& ctx) {
          // skip all format specifiers
          auto it = ctx.begin();
          while (*it != '}')
              ++it;
          return it;
        }

      template<typename FmtContext>
        auto format(const mr::Matr<T, N> &m, FmtContext& ctx) const {
          ostringstream out;
          out << m;
          return ranges::copy(std::move(out).str(), ctx.out()).out;
        }
    };
} // namespace std
#endif

#endif // __Matr_hpp_
