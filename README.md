# 3123B_motor-cautare_BlinduConstantin
În acest repository am publicat proiectul "Motor de Căutare Simplu pentru Documente Text" pentru POO.  Am implementat in acesta opțiuni de căutare avansată, indexarea și căutarea unor cuvinte într-o colecție de documente, eliminarea de stop-words si teste unitare pentru validarea tranzactiilor.

##Alegerea directorului, compilare și rulare
Utilizatorul este nevoit să parcurgă acești pași pentru a rula proiectul în Linux.
•	Alegerea directorului se face cu comanda cd și introducerea numelui ce aparține folderului.
În acest caz, este:
cd ~/motor_cautare

•	Compilarea efectuează acțiunea de „build” necesară pentru a rula programul. Se poate realiza prin două metode:
o	Manual 
g++ -std=c++17 -Wall -O2 -o motor_cautare motor_cautare.cpp
g++ -std=c++17 -Wall -O2 -o teste teste_unitare.cpp
o	Makefile
make all
make test (compilare + rulare teste)

•	Rularea se realizează astfel:
./teste 
./motor_cautare ./documente
