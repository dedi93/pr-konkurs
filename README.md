# Konkurs pieknosci
Pasztetowo zdecydowało się raz na zawsze skończyć z krzywdzącymi stereotypami na temat wyglądu swoich mieszkańców i mieszkanek. W związku z tym sołtys postanowił urządzić konkurs piękności, w którym mogą z mieszkańcami i mieszkankami konkurować także przybysze z zewnątrz.

N menadżerów spontanicznie decyduje, że będzie w tym roku obsługiwać pewną liczbę modelek. Następnie menadżerowie ubiegają się o dostęp do jednego z lekarzy wyznaczonych przez gminę (lekarzy jest L i są rozróżnialni). Badania lekarskie są konieczne od czasu, gdy jacyś dowcipnisie wystawili w konkursie upudrowaną świnię w bikini, która zdobyła tytuł wicemiss Pasztetowa. Po zdobyciu opinii lekarza, menadżerowie rezerwują odpowiednią liczbę miejsc w salonie kosmetycznym (salon ma pojemność S). Następnie zgłaszają gotowość. Gdy wszyscy menadżerowie są gotowi, rozpoczyna się konkurs (poza kontrolą programu).

## Zasoby
* M - liczba modelek
* doc_line[L] - tablica kolejek do lekarzy
* salon_line - kolejka do salonu (przechowuje pary wartosci [id, M] - identyfikator procesu oraz liczba miejsc jaka potrzebuje)
* tablica otrzymanych zgod

## Parametry
L - liczba lekarzy
S - pojemnosc salonu

## Inicjalizacja
Wyzerowanie zegarow lokalnych.
Kazdy proces losuje liczbe modelek M (0 < M <= S).
Wszystkie kolejki puste.

## Opis algorytmu
todo
