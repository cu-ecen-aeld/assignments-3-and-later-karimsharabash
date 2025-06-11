#include "linkedList.h"

// Function implementations from the earlier example
void initList(LinkedList *list) {
    list->head = NULL;
}

void addNode(LinkedList *list, int data) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Failed to allocate memory for new node");
        return;
    }
    newNode->data = data;
    newNode->next = NULL;

    if (list->head == NULL) {
        list->head = newNode;
        return;
    }

    Node *current = list->head;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = newNode;
}

void removeNode(LinkedList *list, int data) {
    if (list->head == NULL) {
        return;
    }

    Node *current = list->head;
    Node *prev = NULL;

    if (current->data == data) {
        list->head = current->next;
        free(current);
        return;
    }

    while (current != NULL && current->data != data) {
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return;
    }

    prev->next = current->next;
    free(current);
}

void displayList(LinkedList *list) {
    if (list->head == NULL) {
        return;
    }

    Node *current = list->head;
    while (current != NULL) {
        current = current->next;
    }
}

void freeList(LinkedList *list) {
    Node *current = list->head;
    while (current != NULL) {
        Node *temp = current;
        current = current->next;
        free(temp);
    }
    list->head = NULL;
}

// Get the head of the linked list
Node* getHead(LinkedList *list) {
    return list->head;
}

// Remove the head node of the linked list
void removeHead(LinkedList *list) {
    if (list->head == NULL) {
        return;
    }

    Node *temp = list->head;  // Store the current head
    list->head = list->head->next;  // Update the head to the next node
    free(temp);  // Free the old head node
}


// Add a new node to the head of the linked list
void addNodeToHead(LinkedList *list, int data) {
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Failed to allocate memory for new node");
        return;
    }
    newNode->data = data;
    newNode->next = list->head; // Point the new node to the current head
    list->head = newNode;       // Update the head to the new node
}