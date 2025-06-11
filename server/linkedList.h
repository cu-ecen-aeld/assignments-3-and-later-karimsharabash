#ifndef LINKED_LIST_H
#define LINKED_LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Node structure for the linked list
typedef struct Node {
    int data;              // Data stored in the node
    struct Node *next;     // Pointer to the next node
} Node;

// Linked list structure
typedef struct LinkedList {
    Node *head;            // Pointer to the head of the list
} LinkedList;

// Function to initialize the linked list
void initList(LinkedList *list);

// Function to add a new node to the linked list
void addNode(LinkedList *list, int data);

// Function to remove a node with the specified data from the linked list
void removeNode(LinkedList *list, int data);

// Function to display the contents of the linked list
void displayList(LinkedList *list);

// Function to free all nodes in the linked list
void freeList(LinkedList *list);

// Function to get the head of the linked list
Node* getHead(LinkedList *list);

// Function to remove the head node of the linked list
void removeHead(LinkedList *list);

// Function to add a new node to the head of the linked list
void addNodeToHead(LinkedList *list, int data);

#endif // LINKED_LIST_H