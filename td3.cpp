#pragma region "Includes"//{
#define _CRT_SECURE_NO_WARNINGS // On permet d'utiliser les fonctions de copies de chaînes qui sont considérées non sécuritaires.

#include "structures.hpp"      // Structures de données pour la collection de films en mémoire.

#include <iostream>
#include <fstream>
#include <string>
#include <limits>
#include <algorithm>
#include <span>

#include "cppitertools/range.hpp"

#include "bibliotheque_cours.hpp"
#include "verification_allocation.hpp" // Nos fonctions pour le rapport de fuites de mémoire.
#include "debogage_memoire.hpp"        // Ajout des numéros de ligne des "new" dans le rapport de fuites.  Doit être après les include du système, qui peuvent utiliser des "placement new" (non supporté par notre ajout de numéros de lignes).

using namespace std;
using namespace iter;

#pragma endregion//}

typedef uint8_t UInt8;
typedef uint16_t UInt16;

#pragma region "Fonctions de base pour lire le fichier binaire"//{

UInt8 lireUint8(istream& fichier)
{
	UInt8 valeur = 0;
	fichier.read((char*)&valeur, sizeof(valeur));
	return valeur;
}
UInt16 lireUint16(istream& fichier)
{
	UInt16 valeur = 0;
	fichier.read((char*)&valeur, sizeof(valeur));
	return valeur;
}
string lireString(istream& fichier)
{
	string texte;
	texte.resize(lireUint16(fichier));
	fichier.read((char*)&texte[0], streamsize(sizeof(texte[0])) * texte.length());
	return texte;
}

#pragma endregion//}

#pragma region "Fonctions diverses du TD2"//{

void ajouterActeur(ListeActeurs& listeActeurs, Acteur* acteur) {

	if (listeActeurs.nElements >= listeActeurs.capacite) {
		if (listeActeurs.capacite == 0) {
			listeActeurs.capacite = 1;
		}
		else {
			listeActeurs.capacite *= 2;
		}
		Acteur** nouvelleListe = new Acteur * [listeActeurs.capacite];
		copy(listeActeurs.elements, listeActeurs.elements + listeActeurs.nElements, nouvelleListe);
		delete[] listeActeurs.elements;
		listeActeurs.elements = nouvelleListe;
	}
	listeActeurs.elements[listeActeurs.nElements++] = acteur;
}

void detruireFilm(Film* film)
{
	for (Acteur* acteur : span(film->acteurs.elements, film->acteurs.nElements)) {
		acteur->joueDans.enleverFilm(film);
		if (acteur->joueDans.getNElements() == 0) {
			cout << "Destruction de l'acteur: " << acteur->nom << endl;
			delete acteur;
		}
	}
	delete[] film->acteurs.elements;
	delete film;
}

void afficherActeur(const Acteur& acteur)
{
	cout << "  " << acteur.nom << ", " << acteur.anneeNaissance << " " << acteur.sexe << endl;
}

void afficherFilm(const Film& film)
{
	cout << "Nom du film: " << film.titre << ", Acteurs:" << endl;
	for (Acteur* acteur : span(film.acteurs.elements, film.acteurs.nElements)) {
		afficherActeur(*acteur);
	}
}

#pragma endregion//}

#pragma region "Fonctions specifiques a la classe ListesFilms"//{

ListeFilms::ListeFilms()
{
	capacite_ = 1;
	nElements_ = 0;
	elements_ = new Film * [capacite_];
}

ListeFilms::ListeFilms(string nomFichier)
{
	ifstream fichier(nomFichier, ios::binary);
	fichier.exceptions(ios::failbit);

	int nElements = lireUint16(fichier);

	capacite_ = 1;
	nElements_ = 0;
	elements_ = new Film * [capacite_];

	for (int i : range(nElements)) {
		cout << "Boucle n." << i + 1 << endl;
		ajouterFilm(lireFilm(fichier));
		cout << "capacite listeFilms: " << capacite_ << endl;
		cout << "nElements listeFilms: " << nElements_ << endl;
	}
}

ListeFilms::~ListeFilms()
{
	for (Film* film : span(elements_, nElements_)) {
		detruireFilm(film);
	}
	delete[] elements_;
}

void ListeFilms::ajouterFilm(Film* film) {

	if (nElements_ >= capacite_) {
		if (capacite_ == 0) {
			capacite_ = 1;
		}
		else {
			capacite_ *= 2;
		}
		Film** nouvelleListe = new Film * [capacite_];
		copy(elements_, elements_ + nElements_, nouvelleListe);
		delete[] elements_;
		elements_ = nouvelleListe;
	}
	elements_[nElements_++] = film;
}

void ListeFilms::enleverFilm(Film* film)
{
	for (int i : range(nElements_)) {
		if (elements_[i] == film) {
			elements_[i] = elements_[nElements_ - 1];
			nElements_--;
			break;
		}
	}
}

Acteur* ListeFilms::trouverActeur(const string& nomActeur) const
{
	for (Film* film : span(elements_, nElements_)) {
		for (Acteur* acteur : span(film->acteurs.elements, film->acteurs.nElements)) {
			if (acteur->nom == nomActeur) {
				return acteur;
			}
		}
	}
	return nullptr;
}

Acteur* ListeFilms::lireActeur(istream& fichier)
{
	Acteur* acteur = new Acteur;

	string nomActeur = lireString(fichier);
	acteur->nom = nomActeur;
	acteur->anneeNaissance = lireUint16(fichier);
	acteur->sexe = lireUint8(fichier);
	if (trouverActeur(nomActeur) != nullptr) {
		cout << "Nom de l'acteur (EXISTANT): " << trouverActeur(nomActeur)->nom << endl;
		delete acteur;
		return trouverActeur(nomActeur);
	}

	cout << "Nom de l'acteur (NOUVEAU): " << acteur->nom << endl;
	return acteur;
}

Film* ListeFilms::lireFilm(istream& fichier)
{
	Film* film = new Film;

	film->titre = lireString(fichier);
	film->realisateur = lireString(fichier);
	film->anneeSortie = lireUint16(fichier);
	film->recette = lireUint16(fichier);
	film->acteurs.capacite = lireUint8(fichier);
	film->acteurs.nElements = 0;
	film->acteurs.elements = new Acteur * [film->acteurs.capacite];

	for (int i : range(film->acteurs.capacite)) {
		ajouterActeur(film->acteurs, lireActeur(fichier));
		film->acteurs.elements[i]->joueDans.ajouterFilm(film);
	}
	return film;
}

void ListeFilms::afficherListeFilms() const
{
	static const string ligneDeSeparation = "\n";
	cout << ligneDeSeparation;
	for (Film* film : span(elements_, nElements_)) {
		afficherFilm(*film);
		cout << ligneDeSeparation;
	}
}

void ListeFilms::afficherFilmographieActeur(const string& nomActeur) const
{
	const Acteur* acteur = trouverActeur(nomActeur);
	if (acteur == nullptr)
		cout << "Aucun acteur de ce nom" << endl;
	else
		acteur->joueDans.afficherListeFilms();
}

int ListeFilms::getNElements()
{
	return nElements_;
}

Film** ListeFilms::getElements()
{
	return elements_;
}

#pragma endregion//}

int main()
{
	bibliotheque_cours::activerCouleursAnsi();  // Permet sous Windows les "ANSI escape code" pour changer de couleurs https://en.wikipedia.org/wiki/ANSI_escape_code ; les consoles Linux/Mac les supportent normalement par défaut.

	static const string ligneDeSeparation = "\n\033[35m════════════════════════════════════════\033[0m\n";

	ListeFilms listeFilms = ListeFilms("films.bin");

	cout << ligneDeSeparation << "Le premier film de la liste est:" << endl;
	cout << endl;
	afficherFilm(*listeFilms.getElements()[0]);

	cout << ligneDeSeparation << "Les films sont:" << endl;
	listeFilms.afficherListeFilms();

	listeFilms.trouverActeur("Benedict Cumberbatch")->anneeNaissance = 1976;

	cout << ligneDeSeparation << "Liste des films où Benedict Cumberbatch joue sont:" << endl;
	listeFilms.afficherFilmographieActeur("Benedict Cumberbatch");

	detruireFilm(listeFilms.getElements()[0]);
	listeFilms.enleverFilm(listeFilms.getElements()[0]);

	cout << ligneDeSeparation << "Les films sont maintenant:" << endl;
	listeFilms.afficherListeFilms();
}