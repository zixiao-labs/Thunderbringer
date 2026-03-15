declare module 'thunderbringer' {
  // ── Repository ─────────────────────────────────────────────────
  export class Repository {
    static open(path: string): Promise<Repository>;
    static init(path: string, bare?: boolean): Promise<Repository>;
    static discover(startPath: string): Promise<string>;

    path(): string;
    workdir(): string | null;
    isBare(): boolean;
    isEmpty(): boolean;
    state(): number;
    head(): { name: string; oid: string } | null;
    headDetached(): boolean;
    headUnborn(): boolean;
    free(): void;
  }

  // ── Signature ──────────────────────────────────────────────────
  export interface SignatureData {
    name: string;
    email: string;
    time: number;
    offset: number;
  }

  export namespace Signature {
    function create(name: string, email: string): SignatureData;
    function createWithTime(name: string, email: string, time: number, offset: number): SignatureData;
    function _default(repo: Repository): SignatureData;
  }

  // ── Status ─────────────────────────────────────────────────────
  export interface StatusEntry {
    status: number;
    path?: string;
    headToIndex?: { status: number; oldPath?: string; newPath?: string };
    indexToWorkdir?: { status: number; oldPath?: string; newPath?: string };
  }

  export namespace Status {
    function getStatus(repo: Repository, opts?: { flags?: number }): Promise<StatusEntry[]>;
    function getFileStatus(repo: Repository, path: string): number;

    const CURRENT: number;
    const INDEX_NEW: number;
    const INDEX_MODIFIED: number;
    const INDEX_DELETED: number;
    const INDEX_RENAMED: number;
    const INDEX_TYPECHANGE: number;
    const WT_NEW: number;
    const WT_MODIFIED: number;
    const WT_DELETED: number;
    const WT_TYPECHANGE: number;
    const WT_RENAMED: number;
    const WT_UNREADABLE: number;
    const IGNORED: number;
    const CONFLICTED: number;
  }

  // ── Index ──────────────────────────────────────────────────────
  export interface IndexEntry {
    path: string;
    oid: string;
    fileSize: number;
    mode: number;
    flags: number;
    stage: number;
  }

  export class Index {
    add(path: string): void;
    addAll(paths?: string[]): void;
    remove(path: string): void;
    removeAll(paths?: string[]): void;
    clear(): void;
    write(): void;
    writeTree(): string;
    entryCount(): number;
    entries(): IndexEntry[];
    getByPath(path: string, stage?: number): IndexEntry | null;
  }

  // ── Commit ─────────────────────────────────────────────────────
  export interface CommitData {
    oid: string;
    message: string;
    summary: string;
    author: SignatureData;
    committer: SignatureData;
    time: number;
    parentCount: number;
    parents: string[];
    treeOid: string;
  }

  export interface CreateCommitOptions {
    message: string;
    tree: string;
    parents?: string[];
    author?: SignatureData;
    committer?: SignatureData;
    updateRef?: string | null;
  }

  export namespace Commit {
    function create(repo: Repository, options: CreateCommitOptions): Promise<string>;
    function lookup(repo: Repository, oid: string): Promise<CommitData>;
  }

  // ── Revwalk ────────────────────────────────────────────────────
  export interface RevwalkOptions {
    start?: string;
    sort?: number;
    maxCount?: number;
  }

  export namespace Revwalk {
    function walk(repo: Repository, options?: RevwalkOptions): Promise<string[]>;

    const SORT_NONE: number;
    const SORT_TOPOLOGICAL: number;
    const SORT_TIME: number;
    const SORT_REVERSE: number;
  }

  // ── Reference ──────────────────────────────────────────────────
  export interface ReferenceData {
    name: string;
    shorthand?: string;
    type: number;
    target?: string;
    symbolicTarget?: string;
    isBranch: boolean;
    isRemote: boolean;
    isTag: boolean;
    isNote: boolean;
  }

  export namespace Reference {
    function lookup(repo: Repository, name: string): ReferenceData | null;
    function resolve(repo: Repository, name: string): ReferenceData | null;
    function list(repo: Repository): string[];
    function create(repo: Repository, name: string, target: string, force?: boolean): ReferenceData;
    function createSymbolic(repo: Repository, name: string, target: string, force?: boolean): ReferenceData;
    function _delete(repo: Repository, name: string): void;

    const TYPE_INVALID: number;
    const TYPE_DIRECT: number;
    const TYPE_SYMBOLIC: number;
  }

  // ── Branch ─────────────────────────────────────────────────────
  export interface BranchData {
    name: string;
    fullName: string;
    oid?: string;
    isHead?: boolean;
    isLocal?: boolean;
    isRemote?: boolean;
    detached?: boolean;
  }

  export namespace Branch {
    function create(repo: Repository, name: string, targetOid: string, force?: boolean): BranchData;
    function _delete(repo: Repository, name: string, type?: number): void;
    function list(repo: Repository, type?: number): BranchData[];
    function rename(repo: Repository, oldName: string, newName: string, force?: boolean): BranchData;
    function lookup(repo: Repository, name: string, type?: number): BranchData | null;
    function current(repo: Repository): BranchData | null;

    const LOCAL: number;
    const REMOTE: number;
    const ALL: number;
  }

  // ── Diff ───────────────────────────────────────────────────────
  export interface DiffFile {
    path: string;
    oid: string;
    mode: number;
  }

  export interface DiffDelta {
    status: number;
    flags: number;
    oldFile: DiffFile;
    newFile: DiffFile;
  }

  export interface DiffStats {
    filesChanged: number;
    insertions: number;
    deletions: number;
  }

  export interface DiffResult {
    deltas: DiffDelta[];
    stats: DiffStats;
  }

  export namespace Diff {
    function indexToWorkdir(repo: Repository): Promise<DiffResult>;
    function treeToTree(repo: Repository, oldTreeOid: string | null, newTreeOid: string | null): Promise<DiffResult>;
    function treeToIndex(repo: Repository, treeOid?: string): Promise<DiffResult>;

    const DELTA_UNMODIFIED: number;
    const DELTA_ADDED: number;
    const DELTA_DELETED: number;
    const DELTA_MODIFIED: number;
    const DELTA_RENAMED: number;
    const DELTA_COPIED: number;
    const DELTA_IGNORED: number;
    const DELTA_UNTRACKED: number;
    const DELTA_TYPECHANGE: number;
    const DELTA_UNREADABLE: number;
    const DELTA_CONFLICTED: number;
  }

  // ── Patch ──────────────────────────────────────────────────────
  export interface PatchLine {
    origin: number;
    content: string;
    oldLineno: number;
    newLineno: number;
  }

  export interface PatchHunk {
    header: string;
    oldStart: number;
    oldLines: number;
    newStart: number;
    newLines: number;
    lines: PatchLine[];
  }

  export interface PatchResult {
    oldPath: string;
    newPath: string;
    additions: number;
    deletions: number;
    context: number;
    text: string;
    hunks: PatchHunk[];
  }

  export namespace Patch {
    function fromWorkdir(repo: Repository): Promise<PatchResult[]>;
    function fromTrees(repo: Repository, oldTreeOid: string | null, newTreeOid: string | null): Promise<PatchResult[]>;

    const LINE_CONTEXT: number;
    const LINE_ADDITION: number;
    const LINE_DELETION: number;
    const LINE_CONTEXT_EOFNL: number;
    const LINE_ADD_EOFNL: number;
    const LINE_DEL_EOFNL: number;
  }

  // ── Blame ──────────────────────────────────────────────────────
  export interface BlameHunk {
    linesInHunk: number;
    finalCommitId: string;
    finalStartLineNumber: number;
    origCommitId: string;
    origPath: string;
    origStartLineNumber: number;
    boundary: boolean;
    finalSignature?: { name: string; email: string };
    origSignature?: { name: string; email: string };
  }

  export namespace Blame {
    function file(repo: Repository, path: string): Promise<BlameHunk[]>;
    function getHunkByLine(repo: Repository, path: string, line: number): BlameHunk | null;
  }

  // ── Remote ─────────────────────────────────────────────────────
  export interface RemoteInfo {
    name: string;
    url?: string;
    pushUrl?: string;
  }

  export namespace Remote {
    function list(repo: Repository): string[];
    function add(repo: Repository, name: string, url: string): void;
    function _delete(repo: Repository, name: string): void;
    function getUrl(repo: Repository, name: string): RemoteInfo;
    function fetch(repo: Repository, remoteName: string): Promise<void>;
    function push(repo: Repository, remoteName: string, refspecs: string[]): Promise<void>;
  }

  // ── Merge ──────────────────────────────────────────────────────
  export interface MergeAnalysisResult {
    analysis: number;
    preference: number;
    isUpToDate: boolean;
    isFastForward: boolean;
    isNormal: boolean;
  }

  export namespace Merge {
    function analysis(repo: Repository, theirOid: string): Promise<MergeAnalysisResult>;
    function merge(repo: Repository, theirOid: string): Promise<void>;
    function base(repo: Repository, oid1: string, oid2: string): string | null;

    const ANALYSIS_NONE: number;
    const ANALYSIS_NORMAL: number;
    const ANALYSIS_UP_TO_DATE: number;
    const ANALYSIS_FASTFORWARD: number;
    const ANALYSIS_UNBORN: number;
  }

  // ── Tag ────────────────────────────────────────────────────────
  export namespace Tag {
    function create(repo: Repository, name: string, targetOid: string, tagger: SignatureData, message: string): string;
    function createLightweight(repo: Repository, name: string, targetOid: string, force?: boolean): string;
    function _delete(repo: Repository, name: string): void;
    function list(repo: Repository, pattern?: string): string[];
  }

  // ── Stash ──────────────────────────────────────────────────────
  export interface StashEntry {
    index: number;
    message: string;
    oid: string;
  }

  export namespace Stash {
    function save(repo: Repository, stasher: SignatureData, message?: string, flags?: number): string;
    function pop(repo: Repository, index?: number): void;
    function apply(repo: Repository, index?: number): void;
    function drop(repo: Repository, index?: number): void;
    function list(repo: Repository): StashEntry[];

    const DEFAULT: number;
    const KEEP_INDEX: number;
    const INCLUDE_UNTRACKED: number;
    const INCLUDE_IGNORED: number;
    const KEEP_ALL: number;
  }

  // ── Checkout ───────────────────────────────────────────────────
  export interface CheckoutOptions {
    strategy?: number;
  }

  export namespace Checkout {
    function head(repo: Repository, opts?: CheckoutOptions): Promise<void>;
    function branch(repo: Repository, branchName: string, opts?: CheckoutOptions): Promise<void>;
    function reference(repo: Repository, refName: string, opts?: CheckoutOptions): Promise<void>;

    const STRATEGY_NONE: number;
    const STRATEGY_SAFE: number;
    const STRATEGY_FORCE: number;
  }

  // ── Config ─────────────────────────────────────────────────────
  export namespace Config {
    function get(repo: Repository, key: string): string | null;
    function set(repo: Repository, key: string, value: string): void;
    function getInt(repo: Repository, key: string): number | null;
    function getBool(repo: Repository, key: string): boolean | null;
    function setInt(repo: Repository, key: string, value: number): void;
    function setBool(repo: Repository, key: string, value: boolean): void;
    function deleteEntry(repo: Repository, key: string): void;
  }

  // ── Metadata ───────────────────────────────────────────────────
  export const libgit2Version: string;
}
