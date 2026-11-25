#ifndef UIM_DIALOG_H
#define UIM_DIALOG_H
/* uim_dialog.h — the dialog atom: a bounded stack of modal / non-modal popups
 * with resolve results and focus save/restore.
 *
 * What it owns:
 * - open/closed state per dialog, stacking order, modal flag and the
 *   accepted/cancelled/closed result the host dispatches on.
 * - focus handoff: when the first dialog opens the current focus owner is
 *   saved; once the stack empties the host restores that owner.
 * - Esc / close routing: cancel_top() resolves the top dialog as cancelled.
 *
 * What it does not own: drawing, the dim/scrim, keyboard capture, button
 * layout, or the text field inside a dialog (uim_text_input /
 * uim_numeric_field own those). Placement on screen is uim_placement.
 *
 * Contract: fixed capacity (UIM_DIALOG_CAPACITY), no allocation, no globals,
 * no renderer. Opening a duplicate id is uim_ERR_EXISTS and the stack is left
 * unchanged; opening past capacity is uim_ERR_BUFFER.
 */
#include "uim_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UIM_DIALOG_CAPACITY 8u

typedef enum uim_DialogResult {
    uim_DIALOG_RESULT_NONE = 0,
    uim_DIALOG_RESULT_ACCEPTED,
    uim_DIALOG_RESULT_CANCELLED,
    uim_DIALOG_RESULT_CLOSED
} uim_DialogResult;

typedef struct uim_DialogFrame {
    uint32_t id;
    bool modal;
    bool open;
    uim_DialogResult result;
    uint32_t focus_owner;
} uim_DialogFrame;

typedef struct uim_DialogStack {
    uim_DialogFrame frames[UIM_DIALOG_CAPACITY];
    size_t count;
    uint32_t focus_return;
    bool focus_saved;
} uim_DialogStack;

static inline uim_Result uim_dialog_init(uim_DialogStack *stack) {
    if (!stack) return uim_ERR_ARGUMENT;
    stack->count = 0;
    stack->focus_return = 0u;
    stack->focus_saved = false;
    return uim_OK;
}

/* Open a dialog. id must be nonzero and not already open. focus_owner is the
 * widget that had focus before opening (0 = none); it is recorded for focus
 * restore when the stack drains. */
static inline uim_Result uim_dialog_open(uim_DialogStack *stack, uint32_t id, bool modal,
                                         uint32_t focus_owner, size_t *out_index) {
    size_t i;
    if (!stack || id == 0u) return uim_ERR_ARGUMENT;
    for (i = 0; i < stack->count; i++) {
        if (stack->frames[i].id == id && stack->frames[i].open) return uim_ERR_EXISTS;
    }
    if (stack->count >= UIM_DIALOG_CAPACITY) return uim_ERR_BUFFER;
    if (stack->count == 0u) {
        stack->focus_return = focus_owner;
        stack->focus_saved = true;
    }
    stack->frames[stack->count].id = id;
    stack->frames[stack->count].modal = modal;
    stack->frames[stack->count].open = true;
    stack->frames[stack->count].result = uim_DIALOG_RESULT_NONE;
    stack->frames[stack->count].focus_owner = focus_owner;
    if (out_index) *out_index = stack->count;
    stack->count++;
    return uim_OK;
}

static inline bool uim_dialog_is_open(const uim_DialogStack *stack, uint32_t id) {
    size_t i;
    if (!stack || id == 0u) return false;
    for (i = 0; i < stack->count; i++) {
        if (stack->frames[i].id == id && stack->frames[i].open) return true;
    }
    return false;
}

/* Topmost open dialog, if any. */
static inline uim_Result uim_dialog_top(const uim_DialogStack *stack,
                                        const uim_DialogFrame **out_frame) {
    if (!stack || !out_frame) return uim_ERR_ARGUMENT;
    if (stack->count == 0u) return uim_ERR_NOT_FOUND;
    *out_frame = &stack->frames[stack->count - 1u];
    return uim_OK;
}

/* True when the top open dialog is modal: the host must block background
 * interaction while this is true. */
static inline bool uim_dialog_modal_blocked(const uim_DialogStack *stack) {
    size_t i;
    if (!stack || stack->count == 0u) return false;
    for (i = stack->count; i > 0u; i--) {
        const uim_DialogFrame *f = &stack->frames[i - 1u];
        if (f->open) return f->modal;
    }
    return false;
}

/* Record a result and mark the dialog closed. The frame stays in the stack
 * until uim_dialog_dismiss_closed() pops it (so the host can still read the
 * result this frame). */
static inline uim_Result uim_dialog_resolve(uim_DialogStack *stack, uint32_t id,
                                            uim_DialogResult result) {
    size_t i;
    if (!stack || id == 0u) return uim_ERR_ARGUMENT;
    if (result == uim_DIALOG_RESULT_NONE) return uim_ERR_ARGUMENT;
    for (i = 0; i < stack->count; i++) {
        if (stack->frames[i].id == id && stack->frames[i].open) {
            stack->frames[i].result = result;
            stack->frames[i].open = false;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

/* Esc / window-close: resolve the top open dialog as cancelled. */
static inline uim_Result uim_dialog_cancel_top(uim_DialogStack *stack) {
    const uim_DialogFrame *top;
    uim_Result rc = uim_dialog_top(stack, &top);
    if (rc != uim_OK) return rc;
    return uim_dialog_resolve(stack, top->id, uim_DIALOG_RESULT_CANCELLED);
}

/* Default / primary button: resolve the top open dialog as accepted. */
static inline uim_Result uim_dialog_accept_top(uim_DialogStack *stack) {
    const uim_DialogFrame *top;
    uim_Result rc = uim_dialog_top(stack, &top);
    if (rc != uim_OK) return rc;
    return uim_dialog_resolve(stack, top->id, uim_DIALOG_RESULT_ACCEPTED);
}

static inline uim_Result uim_dialog_result(const uim_DialogStack *stack, uint32_t id,
                                          uim_DialogResult *out_result) {
    size_t i;
    if (!stack || !out_result || id == 0u) return uim_ERR_ARGUMENT;
    for (i = 0; i < stack->count; i++) {
        if (stack->frames[i].id == id) {
            *out_result = stack->frames[i].result;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

/* Pop resolved/closed frames from the top down. Closed dialogs are always
 * topmost in a correctly used stack; a closed frame below an open one is
 * left in place. When the last frame pops, focus restore becomes available. */
static inline uim_Result uim_dialog_dismiss_closed(uim_DialogStack *stack) {
    if (!stack) return uim_ERR_ARGUMENT;
    while (stack->count > 0u && !stack->frames[stack->count - 1u].open) {
        stack->count--;
    }
    return uim_OK;
}

/* Focus owner to restore, valid once the stack has drained. Returns
 * uim_ERR_STATE while a dialog is still open. */
static inline uim_Result uim_dialog_focus_to_restore(const uim_DialogStack *stack,
                                                    uint32_t *out_id) {
    if (!stack || !out_id) return uim_ERR_ARGUMENT;
    if (stack->count > 0u) return uim_ERR_STATE;
    if (!stack->focus_saved) return uim_ERR_NOT_FOUND;
    *out_id = stack->focus_return;
    return uim_OK;
}

static inline void uim_dialog_clear(uim_DialogStack *stack) {
    if (!stack) return;
    stack->count = 0;
    stack->focus_return = 0u;
    stack->focus_saved = false;
}

#ifdef __cplusplus
}
#endif
#endif /* UIM_DIALOG_H */
