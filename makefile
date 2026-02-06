# -------------------------------------------------
WAVDIR      := wav-files
WAV_FILES   := $(wildcard $(WAVDIR)/*.wav)

# one generated .c file per .wav
WAV_C_FILES := $(patsubst $(WAVDIR)/%.wav,$(WAVDIR)/%_wav.c,$(WAV_FILES))

WAV_TABLE_H := $(WAVDIR)/wav_table.h
WAV_TABLE_C := $(WAVDIR)/wav_table.c

# -------------------------------------------------
# 4️⃣ Turn each wav into a C array (xxd –i)
$(WAVDIR)/%_wav.c: $(WAVDIR)/%.wav
	@echo "Generating $@ from $<"
	xxd -i $< > $@

# 5️⃣ Compile the generated C files (explicit rule – we keep it)
$(WAVDIR)/%_wav.o: $(WAVDIR)/%_wav.c
	$(CC) $(CFLAGS) -x c -c -o $@ $<

# -------------------------------------------------
# 6️⃣ Header generation
$(WAV_TABLE_H): $(WAV_FILES)
	@echo "Generating $@"
	@printf "// Auto‑generated – do NOT edit manually\n#pragma once\n\n" > $@
	@printf "typedef struct {\n    const char *name;\n    const unsigned char *data;\n    unsigned int size;\n} EmbeddedWav;\n\n" >> $@
	@printf "extern const EmbeddedWav get_embedded_wav(const char *name);\n\n" >> $@
	@for f in $(WAV_FILES); do \
		base=$$(basename $$f .wav); \
		printf "extern unsigned char wav_files_%s_wav[];\nextern unsigned int wav_files_%s_wav_len;\n" "$$base" "$$base"; \
	done >> $@

# -------------------------------------------------
# 7️⃣ Source generation
$(WAV_TABLE_C): $(WAV_TABLE_H) $(WAV_FILES)
	@printf "#include <string.h>\n#include \"wav_table.h\"\n\n" > $@
	@printf "EmbeddedWav get_embedded_wav(const char *name){\n" >> $@
	@for f in $(WAV_FILES); do \
		base=$$(basename $$f .wav); \
		printf "    if (strcmp(name, \"%s\") == 0) { return (EmbeddedWav){ .name = \"%s.wav\", .data = wav_files_%s_wav, .size = wav_files_%s_wav_len }; }\n" \
		       "$$base" "$$base" "$$base" "$$base"; \
	done >> $@
	@printf "    return (EmbeddedWav){0,0,0};\n}\n" >> $@

# -------------------------------------------------
# 8️⃣ Generic compilation rule (adds automatic .d files)
CFLAGS += -Wall -Wextra -O2 -I$(WAVDIR) -std=c23 \
          -MMD -MF $(@:.o=.d)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# -------------------------------------------------
SRC  := tabata.c audio.c $(WAV_TABLE_C) $(WAV_C_FILES)
OBJ  := $(SRC:.c=.o)

# Every object that can refer to the generated header must wait for it
$(OBJ): $(WAV_TABLE_H)

cabata: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) -lsndfile -lportaudio -lasound

-include $(OBJ:.o=.d)

.PHONY: clean install
clean:
	rm -f $(OBJ) cabata $(WAV_C_FILES) $(WAV_TABLE_H) $(WAV_TABLE_C)

install: cabata $(WAV_TABLE_H)
	@echo "Installing binary to $(BINDIR)..."
	@install -d $(BINDIR)
	@install -m 755 cabata $(BINDIR)/
