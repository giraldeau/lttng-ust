#ifndef DYNSEQ_H_
#define DYNSEQ_H_

struct dynseq {
	char *filename;
	size_t filename_len;
	int lineno;
};

#endif /* DYNSEQ_H_ */


