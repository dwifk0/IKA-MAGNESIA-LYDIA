# SPDX-License-Identifier: AGPL-3.0-only
# Masaustunde kosan birim testler — kart, kutuphane ve arac gerekmez.
#
#   make        derle ve kosturur
#   make temiz  ciktilar silinir

BAYRAK = -std=c++17 -Wall -Wextra -Wpedantic -Werror -O1 -g
CEK    = ../cekirdek

KUME = cerceve emniyet ibus direksiyon kip gaz_taret

.PHONY: all temiz
all: $(addprefix kos_,$(KUME))

kos_cerceve: t_cerceve
	@./t_cerceve
kos_emniyet: t_emniyet
	@./t_emniyet
kos_ibus: t_ibus
	@./t_ibus
kos_direksiyon: t_direksiyon
	@./t_direksiyon
kos_kip: t_kip
	@./t_kip
kos_gaz_taret: t_gaz_taret
	@./t_gaz_taret

t_cerceve: test_cerceve.cpp $(CEK)/cerceve.cpp
	@g++ $(BAYRAK) -o $@ $^
t_emniyet: test_emniyet.cpp $(CEK)/emniyet.cpp
	@g++ $(BAYRAK) -o $@ $^
t_ibus: test_ibus.cpp $(CEK)/ibus.cpp
	@g++ $(BAYRAK) -o $@ $^
t_direksiyon: test_direksiyon.cpp $(CEK)/direksiyon_jog.cpp
	@g++ $(BAYRAK) -o $@ $^
t_kip: test_kip.cpp $(CEK)/kip_hakemi.cpp
	@g++ $(BAYRAK) -o $@ $^
t_gaz_taret: test_gaz_taret.cpp $(CEK)/gaz_profili.cpp $(CEK)/taret_jog.cpp
	@g++ $(BAYRAK) -o $@ $^

temiz:
	@rm -f t_cerceve t_emniyet t_ibus t_direksiyon t_kip t_gaz_taret
