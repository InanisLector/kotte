# Bomberman World — [your game title]

**Student:** Kuznietsov Illia

# Compilation
Since I have replaced raylib with submodules for it to be compilable on linux, you have to clone submodules first
##### If you haven't cloned yet:
- git clone --recurse-submodules https://github.com/InanisLector/kotte.git
##### If you have:
- git submodule update --init --recursive

##### Then just use cmake to compile it
- cmake --build --preset windows

> Replace the prompts below and remove these instructions. Keep answers brief. Your implementation explanations and reflection belong in the presentation; you do not need to write them out here. Complete the final section only if seeking Pass with Distinction (VG).

## How to play

- **Controls:** [movement, bombs, restart, and any extension controls]
- **Objective and rules:** [how to win/lose; which result wins if both happen together; any extension rules needed to understand the game]
- **Configurations / demo setup:** [how to select gameplay/stress configurations and use any special setup or debug keys; write “default” where appropriate]

## Features and validation

(delete this pre-amble before submitting the assignment) Complete the five required features and three extensions of your choice. For each row, document **at least one useful check: what you tested, what you observed, and how the result compares with the expected behaviour**. One or two sentences per feature are enough. Choose a check that could reveal a mistake, such as a boundary case or interaction.

Use the practical checks in the assignment to validate the acceptance conditions during development; this table is a small sample of that work. Manual play in a small, repeatable setup is sufficient. You can also use a debugger, temporary logging, or small scenarios set up in code. Screenshots, recordings, automated tests etc are welcome, but not required. Be ready to explain the relevant rules in your code.

| Feature | Check, observed result, and expected behaviour |
| --- | --- |
| 1. Player damage and death | [what I tested, what I observed, and whether it matched the expected behaviour] |
| 2. Round states and objective | [what I tested, what I observed, and whether it matched the expected behaviour] |
| 3. Safe restart | [what I tested, what I observed, and whether it matched the expected behaviour] |
| 4. Solid bombs with owner escape | [what I tested, what I observed, and whether it matched the expected behaviour] |
| 5. Blast-range pickup | [what I tested, what I observed, and whether it matched the expected behaviour] |
| [# and name of chosen extension 1] | [what I tested, what I observed, and whether it matched the expected behaviour] |
| [# and name of chosen extension 2] | [what I tested, what I observed, and whether it matched the expected behaviour] |
| [# and name of chosen extension 3] | [what I tested, what I observed, and whether it matched the expected behaviour] |

Example: “I ended a round with live bombs and restarted three times to check that old-round state was cleared. No old explosions appeared, and bomb capacity reset each time.” Report your actual results, including any failures or remaining limitations; stating that something “works” does not describe a check.

**Known issues:** [remaining problems and how to encounter them, or “None known after testing”]

## Sources and assistance

This project builds on the shared Kotte foundations.

[Briefly identify meaningful external code/resources, discussion or collaboration, and AI assistance. Link to sources where relevant, say what they contributed, and explain what you checked yourself. State “None beyond the course material” if applicable. No prompt transcript or activity diary is needed. Retain required licence/attribution notices.]

## Architectural investigation — VG only

Delete this section for a G submission. For VG, pose a focused engineering question about one decision in your implementation, investigate it, and draw a conclusion from evidence. Documenting the explanation already required for G is not enough. Aim for half a page to one page, excluding small tables, diagrams, or code excerpts. You can use the same decision in your presentation too.

### Decision

[State the question you are investigating and the implementation decision it concerns. Describe your current approach and a plausible alternative. Identify the relevant code. You do not need to implement the alternative.]

### Method

[How did you investigate the decision? Give enough detail to reproduce the key observation: setup, input, configuration/seed, and the test, trace, debugger inspection, or other method used.]

### Evidence

[What did you actually observe in your game? Include the relevant result or a small excerpt, table, or link to supporting material.]

### Interpretation

[Explain why it happened using the relevant code or architecture. Analyse at least one edge case, failure, defect, limitation, or trade-off illustrated by the investigation. Discuss relevant consequences for correctness, maintainability, performance, and/or scalability; you do not need to cover all four.]

### Conclusion

[When is your approach appropriate, what limits it, and when might an alternative become preferable? Base the conclusion on your evidence. Confirming that the current approach is suitable is a valid result.]

<!-- Submission: commit and push this README with your source to GitHub or the internal Gitea server. Submit the final commit link and a runnable Release ZIP from that version in Studium. Ensure the teacher can access the repository throughout assessment. -->
