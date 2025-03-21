﻿/*
 * Nom: Fichier Source TD4
 * Auteurs: Manuel Jarry et Meryem El Kamouni
 */

#pragma region "Includes"//{
#define _CRT_SECURE_NO_WARNINGS // On permet d'utiliser les fonctions de copies de chaînes qui sont considérées non sécuritaires.

#include "structures.hpp"      // Structures de données pour la collection de films en mémoire.

#include "bibliotheque_cours.hpp"
#include "verification_allocation.hpp" // Nos fonctions pour le rapport de fuites de mémoire.

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <limits>
#include <algorithm>
#include <sstream>
#include <span>
#include <vector>
#include "cppitertools/range.hpp"
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

// Fonctions pour ajouter un Film à une ListeFilms.
//[
void ListeFilms::changeDimension(int nouvelleCapacite)
{
	Film** nouvelleListe = new Film * [nouvelleCapacite];

	if (elements != nullptr) {  // Noter que ce test n'est pas nécessaire puique nElements sera zéro si elements est nul, donc la boucle ne tentera pas de faire de copie, et on a le droit de faire delete sur un pointeur nul (ça ne fait rien).
		nElements = min(nouvelleCapacite, nElements);
		for (int i : range(nElements))
			nouvelleListe[i] = elements[i];
		delete[] elements;
	}

	elements = nouvelleListe;
	capacite = nouvelleCapacite;
}

void ListeFilms::ajouterFilm(Film* film)
{
	if (nElements == capacite)
		changeDimension(max(1, capacite * 2));
	elements[nElements++] = film;
}
//]

// Fonction pour enlever un Film d'une ListeFilms (enlever le pointeur) sans effacer le film; la fonction prenant en paramètre un pointeur vers le film à enlever.  L'ordre des films dans la liste n'a pas à être conservé.
//[
// On a juste fait une version const qui retourne un span non const.  C'est valide puisque c'est la struct qui est const et non ce qu'elle pointe.  Ça ne va peut-être pas bien dans l'idée qu'on ne devrait pas pouvoir modifier une liste const, mais il y aurais alors plusieurs fonctions à écrire en version const et non-const pour que ça fonctionne bien, et ce n'est pas le but du TD (il n'a pas encore vraiment de manière propre en C++ de définir les deux d'un coup).
span<Film*> ListeFilms::enSpan() const { return span(elements, nElements); }

void ListeFilms::enleverFilm(const Film* film)
{
	for (Film*& element : enSpan()) {  // Doit être une référence au pointeur pour pouvoir le modifier.
		if (element == film) {
			if (nElements > 1)
				element = elements[nElements - 1];
			nElements--;
			return;
		}
	}
}
//]

// Fonction pour trouver un Acteur par son nom dans une ListeFilms, qui retourne un pointeur vers l'acteur, ou nullptr si l'acteur n'est pas trouvé.  Devrait utiliser span.
//[

//NOTE: Doit retourner un Acteur modifiable, sinon on ne peut pas l'utiliser pour modifier l'acteur tel que demandé dans le main, et on ne veut pas faire écrire deux versions.
shared_ptr<Acteur> ListeFilms::trouverActeur(const string& nomActeur) const
{
	for (const Film* film : enSpan()) {
		for (const shared_ptr<Acteur>& acteur : film->acteurs_.enSpan()) {
			if (acteur->nom == nomActeur)
				return acteur;
		}
	}
	return nullptr;
}
//]

// Les fonctions pour lire le fichier et créer/allouer une ListeFilms.

shared_ptr<Acteur> lireActeur(istream& fichier, const ListeFilms& listeFilms)
{
	Acteur acteur = {};
	acteur.nom = lireString(fichier);
	acteur.anneeNaissance = lireUint16(fichier);
	acteur.sexe = lireUint8(fichier);

	shared_ptr<Acteur> acteurExistant = listeFilms.trouverActeur(acteur.nom);
	if (acteurExistant != nullptr)
		return acteurExistant;
	else {
		cout << "Création Acteur " << acteur.nom << endl;
		return make_shared<Acteur>(move(acteur));  // Le move n'est pas nécessaire mais permet de transférer le texte du nom sans le copier.
	}
}

Film* lireFilm(istream& fichier, ListeFilms& listeFilms)
{
	auto titre = lireString(fichier);
	auto realisateur = lireString(fichier);
	auto anneeSortie = lireUint16(fichier);
	auto recette = lireUint16(fichier);
	auto nActeurs = lireUint8(fichier);
	Film* film = new Film(titre, realisateur, anneeSortie, recette, nActeurs);

	for ([[maybe_unused]] auto i : range(nActeurs)) {  // On peut aussi mettre nElements avant et faire un span, comme on le faisait au TD précédent.
		film->acteurs_.ajouter(lireActeur(fichier, listeFilms));
	}

	return film;
}

ListeFilms creerListe(string nomFichier)
{
	ifstream fichier(nomFichier, ios::binary);
	fichier.exceptions(ios::failbit);

	int nElements = lireUint16(fichier);

	ListeFilms listeFilms;
	for ([[maybe_unused]] int i : range(nElements)) { //NOTE: On ne peut pas faire un span simple avec ListeFilms::enSpan car la liste est vide et on ajoute des éléments à mesure.
		listeFilms.ajouterFilm(lireFilm(fichier, listeFilms));
	}

	return listeFilms;
}

// Fonction pour détruire une ListeFilms et tous les films qu'elle contient.
//[
//NOTE: La bonne manière serait que la liste sache si elle possède, plutôt qu'on le dise au moment de la destruction, et que ceci soit le destructeur.  Mais ça aurait complexifié le TD2 de demander une solution de ce genre, d'où le fait qu'on a dit de le mettre dans une méthode.
void ListeFilms::detruire(bool possedeLesFilms)
{
	if (possedeLesFilms)
		for (Film* film : enSpan())
			delete film;
	delete[] elements;
}
//]

void afficherBibliotheque(vector<Item*> bibliotheque)
{
	for (auto&& item : bibliotheque) {
		if (auto film = dynamic_cast<Film*>(item)) {
			cout << *film << endl;
		}
		else if (auto livre = dynamic_cast<Livre*>(item)) {
			cout << *livre << endl;
		}
	}
}

void detruireLivresBibliotheque(vector<Item*> bibliotheque)
{
	for (auto&& item : bibliotheque) {
		if (auto livre = dynamic_cast<Livre*>(item)) {
			if (livre != nullptr) {
				delete livre;
			}
		}
	}
}

bool Item::titreContient(const string& texte) {
	if (titre_.contains(texte)) {
		return true;
	}
	else {
		return false;
	}
}

Film& Film::operator= (const Film& film) {
	if (this != &film) {
		Item::operator=(film);
		realisateur_ = film.realisateur_;
		recette_ = film.recette_;
		acteurs_ = ListeActeurs(film.acteurs_);
	}
	return *this;
}

ostream& operator<< (ostream& os, const Item& item)
{
	return item.afficher(os);
}

ostream& operator<< (ostream& os, const Film& film)
{
	return film.afficher(os);
}

ostream& operator<< (ostream& os, const Livre& livre)
{
	return livre.afficher(os);
}

ostream& operator<< (ostream& os, const FilmLivre& filmLivre)
{
	return filmLivre.afficher(os);
}

ostream& operator<< (ostream& os, const Acteur& acteur)
{
	return os << "  " << acteur.nom << ", " << acteur.anneeNaissance << " " << acteur.sexe << endl;
}

ostream& operator<< (ostream& os, const ListeFilms& listeFilms)
{
	static const string ligneDeSeparation = //[
		"\033[32m────────────────────────────────────────\033[0m\n";
	os << ligneDeSeparation;
	for (const Film* film : listeFilms.enSpan()) {
		os << *film << ligneDeSeparation;
	}
	return os;
}

ostream& Item::afficher(ostream& os) const
{
	os << "Titre: " << titre_ << endl;
	os << "Année : " << anneeSortie_ << endl;
	return os;
}

ostream& Film::afficher(ostream& os) const
{
	Item::afficher(os);
	os << "Réalisateur: " << realisateur_ << endl;
	os << "Recette: " << recette_ << "M$" << endl;

	os << "Acteurs:" << endl;
	for (const shared_ptr<Acteur>& acteur : acteurs_.enSpan())
		os << *acteur;
	return os;
}


ostream& Livre::afficher(ostream& os) const
{
	Item::afficher(os);
	os << "Auteur: " << auteur_ << endl;
	os << "Copies vendues: " << copiesVendues_ << "M" << endl;
	os << "Nombre de pages: " << nPages_ << endl;
	return os;
}

ostream& Livre::afficherSansAttributsItem(ostream& os) const
{
	os << "Auteur: " << auteur_ << endl;
	os << "Copies vendues: " << copiesVendues_ << "M" << endl;
	os << "Nombre de pages: " << nPages_ << endl;
	return os;
}

ostream& FilmLivre::afficher(ostream& os) const
{
	Film::afficher(os);
	Livre::afficherSansAttributsItem(os);
	return os;
}

int main()
{
#ifdef VERIFICATION_ALLOCATION_INCLUS
	bibliotheque_cours::VerifierFuitesAllocations verifierFuitesAllocations;
#endif
	bibliotheque_cours::activerCouleursAnsi();  // Permet sous Windows les "ANSI escape code" pour changer de couleurs https://en.wikipedia.org/wiki/ANSI_escape_code ; les consoles Linux/Mac les supportent normalement par défaut.

	static const string ligneDeSeparation = "\n\033[35m════════════════════════════════════════\033[0m\n";

	ListeFilms listeFilms = creerListe("films.bin");

	listeFilms.trouverActeur("Benedict Cumberbatch")->anneeNaissance = 1976;

	vector<Item*> bibliotheque;
	for (int i : range(listeFilms.size())) {
		bibliotheque.push_back(listeFilms[i]);
	}

	ifstream fichierLivres("livres.txt");

	if (fichierLivres.is_open()) {
		string titre;
		int anneeSortie;
		string auteur;
		int copiesVendues;
		int nPages;

		while (fichierLivres >> quoted(titre) >> anneeSortie >> quoted(auteur) >> copiesVendues >> nPages) {
			Livre* livre = new Livre(titre, anneeSortie, auteur, copiesVendues, nPages);
			bibliotheque.push_back(livre);
		}

		fichierLivres.close();
	}

	cout << ligneDeSeparation << "Affichage du contenu de la bibliothèque avant le combo:\n" << endl;
	afficherBibliotheque(bibliotheque);

	Film filmFilmLivre = Film();
	Livre livreFilmLivre = Livre();
	for (auto&& item : bibliotheque) {
		if ((*item).titreContient("Hobbit")) {
			if (auto film = dynamic_cast<Film*>(item)) {
				filmFilmLivre = *film;
			}
			else if (auto livre = dynamic_cast<Livre*>(item)) {
				livreFilmLivre = *livre;
			}
		}
	}

	FilmLivre* filmLivre = new FilmLivre(filmFilmLivre, livreFilmLivre);
	bibliotheque.push_back(filmLivre);

	cout << ligneDeSeparation << "Affichage du contenu de la bibliothèque après le combo:\n" << endl;
	afficherBibliotheque(bibliotheque);

	detruireLivresBibliotheque(bibliotheque);
	listeFilms.detruire(true);
}
