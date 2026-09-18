"""Model checks shared by training, merging and evaluation."""


def check_tokenizer_embeddings(tokenizer, num_embeddings: int) -> None:
    """Allow padded embedding tables, but require a row for every token ID."""
    ids = list(tokenizer.get_vocab().values())
    if not ids or min(ids) < 0 or max(ids) >= num_embeddings:
        raise ValueError(
            f"tokenizer IDs do not fit the model's {num_embeddings} embedding rows"
        )
