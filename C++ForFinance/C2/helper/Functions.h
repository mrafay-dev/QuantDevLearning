#ifndef Functions_H
#define Functions_H


template <class Numeric>
Numeric Add(const Numeric& x, const Numeric& y) {
	return x + y;
}
template <class Numeric>
Numeric Subtract(const Numeric& x, const Numeric& y) {
	return x - y;
}

template <class Numeric>
Numeric Add(const Numeric& x, const Numeric& y, const Numeric& z) {
	return x + y + z;
}
template <class Numeric>
Numeric Subtract(const Numeric& x, const Numeric& y, const Numeric& z) {
	return x - y - z;
}

#endif
