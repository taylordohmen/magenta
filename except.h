#ifndef EXCEPT_H
#define EXCEPT_H

class break_except : public std::exception {
	public:
		break_except() {};
		~break_except() {};
};

class next_except : public std::exception {
	public:
		next_except() {};
		~next_except() {};
};

class return_except : public std::exception {
	public:
		return_except() {};
		~return_except() {};
};

#endif
