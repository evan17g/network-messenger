PRAGMA foreign_keys = ON;

CREATE TABLE Users (
    user_id INTEGER PRIMARY KEY,
    user_name TEXT NOT NULL UNIQUE
);

CREATE TABLE Conversations (
    conversation_id INTEGER PRIMARY KEY,
    user_id_1 INTEGER NOT NULL,
    user_id_2 INTEGER NOT NULL,
    FOREIGN KEY (user_id_1) REFERENCES Users(user_id),
    FOREIGN KEY (user_id_2) REFERENCES Users(user_id),
    CHECK (user_id_1 < user_id_2),
    UNIQUE (user_id_1, user_id_2)
);

CREATE TABLE Messages (
    message_id INTEGER PRIMARY KEY,
    sender_id INTEGER NOT NULL,
    conversation_id INTEGER NOT NULL,
    message TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (sender_id) REFERENCES Users(user_id),
    FOREIGN KEY (conversation_id) REFERENCES Conversations(conversation_id)
);

CREATE INDEX idx_messages_conversation_time ON Messages(conversation_id, created_at);
CREATE INDEX idx_messages_sender ON Messages(sender_id);